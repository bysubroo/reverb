#pragma once

// Multi-input / multi-output partitioned convolution with instant,
// click-free IR replacement.
//
//  * Head: uniform partitions of 256 samples covering the first 8192 taps.
//    Every input row (one per "FDL" input) has its own head IR per output.
//  * Tail: partitions of 4096 samples for taps 8192 .. end, driven by FDL 0
//    (mono sum). The tail work is spread evenly over 16 head blocks, so the
//    CPU load is flat (no spikes).
//  * Because the input spectra history (frequency-domain delay line) does not
//    depend on the IR, a new IR set produces full steady-state output
//    immediately. Old and new outputs are crossfaded over one tail period.
//
// Latency: 256 samples (reported to the host).

#include "FFT.h"
#include <atomic>
#include <memory>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>

namespace srr
{
constexpr int kHeadBlock = 256;
constexpr int kHeadFFT = 2 * kHeadBlock;
constexpr int kHeadParts = 32;
constexpr int kHeadLength = kHeadBlock * kHeadParts;   // 8192
constexpr int kTailBlock = 4096;
constexpr double kMaxIRSeconds = 10.0;                 // longest response the convolver holds
constexpr int kTailFFT = 2 * kTailBlock;
constexpr int kBlocksPerPeriod = kTailBlock / kHeadBlock;
static_assert (kHeadLength == 2 * kTailBlock, "tail must start two tail blocks in");

struct IRSet
{
    int numOutputs = 0;
    int numRows = 0;
    int tailParts = 0;
    std::vector<int> rowFdl;          // which engine input feeds each row
    AlignedBuffer head;               // [row][out][part][kHeadFFT]
    AlignedBuffer tail;               // [out][part][kTailFFT]
    std::vector<int> headPartsUsed;   // [row * numOutputs + out]
    std::vector<int> tailPartsUsed;   // [out]
    float flatGain = 1.0f;            // gain that would normalise this IR ("Flat" level range)
    uint64_t serial = 0;

    float* headSpectrum (int r, int o, int p) { return head.get() + ((size_t) ((r * numOutputs + o) * kHeadParts + p)) * kHeadFFT; }
    float* tailSpectrum (int o, int p)        { return tail.get() + ((size_t) (o * tailParts + p)) * kTailFFT; }

    // headRows: (fdl index, buffer with numOutputs channels; taps 0..kHeadLength used)
    // tailSource: buffer with numOutputs channels; taps from kHeadLength on are used (may be null)
    static std::unique_ptr<IRSet> build (const std::vector<std::pair<int, const juce::AudioBuffer<float>*>>& headRows,
                                         const juce::AudioBuffer<float>* tailSource, int numOutputs, int maxTailParts,
                                         float tailScale = 1.0f);
};

class MatrixConvolver
{
public:
    MatrixConvolver();
    ~MatrixConvolver();

    // not real-time safe
    void prepare (int numFdlInputs, int numOutputs, int maxTailParts);
    void releaseAll();

    int getLatencySamples() const { return kHeadBlock; }
    int getNumOutputs() const { return numOuts; }
    int getNumFdlInputs() const { return numFdl; }
    int getMaxTailParts() const { return maxParts; }

    // background thread: offer a new IR set (latest wins)
    void submit (std::unique_ptr<IRSet> set);
    // background thread: free sets the audio thread no longer needs
    void collectGarbage();

    // audio thread
    void process (const float* const* fdlInputs, float* const* outputs, int numSamples, float levelRange);

    bool hasActiveSet() const { return activeSerial.load() != 0; }
    uint64_t getActiveSerial() const { return activeSerial.load(); }

private:
    void processBlock();
    void periodBoundary();
    void runTailWork (int budgetUnits, bool finish);
    void computeHead (IRSet* set, int o, float* dst);
    void retire (IRSet* s);
    float gainFor (const IRSet* s) const;

    int numFdl = 0, numOuts = 0, maxParts = 0;
    std::unique_ptr<RealFFT> headFFT, tailFFT;

    std::vector<AlignedBuffer> inBlock;        // per fdl, kHeadFFT (prev | current)
    std::vector<AlignedBuffer> headFdl;        // per fdl, kHeadParts * kHeadFFT
    int headPos = 0;
    int inPos = 0;
    std::vector<std::vector<float>> outFifo;   // per out, kHeadBlock

    AlignedBuffer tailIn;                      // kTailFFT: [previous block | current block]
    AlignedBuffer tailFdl;                     // maxParts * kTailFFT
    int tailPos = 0;
    std::vector<AlignedBuffer> tailPlay, tailCompute;   // per out, kTailBlock
    std::vector<AlignedBuffer> accTo, accFrom;          // per out, kTailFFT
    AlignedBuffer work, tmpHead, accHead, tmpTail;
    std::vector<float> yA, yB;
    int blockInPeriod = 0;

    struct Job
    {
        IRSet* from = nullptr;
        IRSet* to = nullptr;
        float gFrom = 0, gTo = 0;
        int stage = 0;        // 0 = fft, 1 = outputs, 2 = done
        int out = 0, slot = 0, part = 0;
        int totalUnits = 0, doneUnits = 0;
    } job;

    IRSet* active = nullptr;
    IRSet* headFrom = nullptr;
    bool headFading = false;
    IRSet* jobFromPrev = nullptr;
    float levelRangeNow = 1.0f;

    std::atomic<IRSet*> pending { nullptr };
    std::atomic<uint64_t> activeSerial { 0 };

    static constexpr int kTrashSize = 64;
    juce::AbstractFifo trashFifo { kTrashSize };
    std::array<IRSet*, kTrashSize> trash {};
};

} // namespace srr
