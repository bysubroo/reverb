#include "MatrixConvolver.h"

namespace srr
{
//==============================================================================
std::unique_ptr<IRSet> IRSet::build (const std::vector<std::pair<int, const juce::AudioBuffer<float>*>>& headRows,
                                     const juce::AudioBuffer<float>* tailSource, int numOutputs, int maxTailParts,
                                     float tailScale)
{
    auto set = std::make_unique<IRSet>();
    set->numOutputs = numOutputs;
    set->numRows = (int) headRows.size();

    int tailLen = 0;
    if (tailSource != nullptr)
        tailLen = std::max (0, tailSource->getNumSamples() - kHeadLength);
    set->tailParts = std::min (maxTailParts, (tailLen + kTailBlock - 1) / kTailBlock);

    set->head.allocate ((size_t) (set->numRows * numOutputs * kHeadParts * kHeadFFT));
    set->tail.allocate ((size_t) std::max (1, numOutputs * set->tailParts * kTailFFT));
    set->headPartsUsed.assign ((size_t) (set->numRows * numOutputs), 0);
    set->tailPartsUsed.assign ((size_t) numOutputs, 0);

    RealFFT hf (kHeadFFT), tf (kTailFFT);
    AlignedBuffer time (kTailFFT), work (kTailFFT);

    for (int r = 0; r < set->numRows; ++r)
    {
        set->rowFdl.push_back (headRows[(size_t) r].first);
        const auto* buf = headRows[(size_t) r].second;
        for (int o = 0; o < numOutputs; ++o)
        {
            if (buf == nullptr || o >= buf->getNumChannels()) continue;
            const float* src = buf->getReadPointer (o);
            const int n = std::min (buf->getNumSamples(), kHeadLength);
            int used = 0;
            for (int p = 0; p < kHeadParts; ++p)
            {
                const int s0 = p * kHeadBlock;
                if (s0 >= n) break;
                time.clear();
                bool nz = false;
                for (int i = 0; i < kHeadBlock && s0 + i < n; ++i)
                {
                    auto v = src[s0 + i] / (float) kHeadFFT;
                    time[(size_t) i] = v;
                    nz = nz || v != 0.0f;
                }
                if (! nz) continue;
                hf.forward (time.get(), set->headSpectrum (r, o, p), work.get());
                used = p + 1;
            }
            set->headPartsUsed[(size_t) (r * numOutputs + o)] = used;
        }
    }

    if (tailSource != nullptr && set->tailParts > 0)
    {
        for (int o = 0; o < numOutputs && o < tailSource->getNumChannels(); ++o)
        {
            const float* src = tailSource->getReadPointer (o);
            const int n = tailSource->getNumSamples();
            int used = 0;
            for (int p = 0; p < set->tailParts; ++p)
            {
                const int s0 = kHeadLength + p * kTailBlock;
                if (s0 >= n) break;
                time.clear();
                bool nz = false;
                for (int i = 0; i < kTailBlock && s0 + i < n; ++i)
                {
                    auto v = src[s0 + i] * tailScale / (float) kTailFFT;
                    time[(size_t) i] = v;
                    nz = nz || v != 0.0f;
                }
                if (nz) used = p + 1;
                tf.forward (time.get(), set->tailSpectrum (o, p), work.get());
            }
            set->tailPartsUsed[(size_t) o] = used;
        }
    }
    return set;
}

//==============================================================================
MatrixConvolver::MatrixConvolver()
    : headFFT (std::make_unique<RealFFT> (kHeadFFT)), tailFFT (std::make_unique<RealFFT> (kTailFFT))
{
}

MatrixConvolver::~MatrixConvolver()
{
    releaseAll();
}

void MatrixConvolver::releaseAll()
{
    collectGarbage();
    std::vector<IRSet*> owned { active, headFrom, jobFromPrev, job.from, job.to, pending.exchange (nullptr) };
    std::sort (owned.begin(), owned.end());
    owned.erase (std::unique (owned.begin(), owned.end()), owned.end());
    for (auto* s : owned) delete s;
    active = headFrom = jobFromPrev = nullptr;
    job = {};
    job.stage = 2;
    headFading = false;
    activeSerial = 0;
}

void MatrixConvolver::prepare (int numFdlInputs, int numOutputs, int maxTailParts)
{
    releaseAll();
    numFdl = std::max (1, numFdlInputs);
    numOuts = std::max (1, numOutputs);
    maxParts = std::max (1, maxTailParts);

    inBlock.clear(); headFdl.clear();
    for (int i = 0; i < numFdl; ++i)
    {
        inBlock.emplace_back ((size_t) kHeadFFT);
        headFdl.emplace_back ((size_t) (kHeadParts * kHeadFFT));
    }
    outFifo.assign ((size_t) numOuts, std::vector<float> ((size_t) kHeadBlock, 0.0f));

    tailIn.allocate (kTailFFT);
    tailFdl.allocate ((size_t) (maxParts * kTailFFT));
    tailPlay.clear(); tailCompute.clear(); accTo.clear(); accFrom.clear();
    for (int o = 0; o < numOuts; ++o)
    {
        tailPlay.emplace_back ((size_t) kTailBlock);
        tailCompute.emplace_back ((size_t) kTailBlock);
        accTo.emplace_back ((size_t) kTailFFT);
        accFrom.emplace_back ((size_t) kTailFFT);
    }
    work.allocate (kTailFFT);
    tmpHead.allocate (kHeadFFT);
    accHead.allocate (kHeadFFT);
    tmpTail.allocate (kTailFFT);
    yA.assign ((size_t) kHeadBlock, 0.0f);
    yB.assign ((size_t) kHeadBlock, 0.0f);

    headPos = 0; inPos = 0; tailPos = 0; blockInPeriod = 0;
    job = {};
    job.stage = 2;
}

void MatrixConvolver::submit (std::unique_ptr<IRSet> set)
{
    if (set == nullptr) return;
    auto* old = pending.exchange (set.release());
    delete old;   // never seen by the audio thread
}

void MatrixConvolver::collectGarbage()
{
    int s1, n1, s2, n2;
    trashFifo.prepareToRead (trashFifo.getNumReady(), s1, n1, s2, n2);
    for (int i = 0; i < n1; ++i) { delete trash[(size_t) (s1 + i)]; trash[(size_t) (s1 + i)] = nullptr; }
    for (int i = 0; i < n2; ++i) { delete trash[(size_t) (s2 + i)]; trash[(size_t) (s2 + i)] = nullptr; }
    trashFifo.finishedRead (n1 + n2);
}

void MatrixConvolver::retire (IRSet* s)
{
    if (s == nullptr || s == active || s == job.from || s == job.to || (headFading && s == headFrom)) return;
    int s1, n1, s2, n2;
    trashFifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0) trash[(size_t) s1] = s;
    else if (n2 > 0) trash[(size_t) s2] = s;
    // (if the fifo were full the set would leak rather than be freed on the audio thread)
    trashFifo.finishedWrite (n1 + n2);
}

float MatrixConvolver::gainFor (const IRSet* s) const
{
    if (s == nullptr) return 0.0f;
    return std::pow (s->flatGain, 1.0f - levelRangeNow);
}

//==============================================================================
void MatrixConvolver::process (const float* const* in, float* const* out, int numSamples, float levelRange)
{
    levelRangeNow = juce::jlimit (0.0f, 1.0f, levelRange);
    int done = 0;
    while (done < numSamples)
    {
        const int take = std::min (numSamples - done, kHeadBlock - inPos);
        for (int i = 0; i < numFdl; ++i)
        {
            auto* dst = inBlock[(size_t) i].get() + kHeadBlock + inPos;
            if (in[i] != nullptr) std::memcpy (dst, in[i] + done, (size_t) take * sizeof (float));
            else std::memset (dst, 0, (size_t) take * sizeof (float));
        }
        for (int o = 0; o < numOuts; ++o)
            if (out[o] != nullptr)
                std::memcpy (out[o] + done, outFifo[(size_t) o].data() + inPos, (size_t) take * sizeof (float));

        inPos += take;
        done += take;
        if (inPos == kHeadBlock)
        {
            processBlock();
            inPos = 0;
        }
    }
}

void MatrixConvolver::computeHead (IRSet* set, int o, float* dst)
{
    std::fill (dst, dst + kHeadBlock, 0.0f);
    if (set == nullptr) return;
    accHead.clear();
    bool any = false;
    for (int r = 0; r < set->numRows; ++r)
    {
        const int fdl = set->rowFdl[(size_t) r];
        if (fdl < 0 || fdl >= numFdl) continue;
        const int used = set->headPartsUsed[(size_t) (r * set->numOutputs + o)];
        const float* ring = headFdl[(size_t) fdl].get();
        for (int p = 0; p < used; ++p)
        {
            const int idx = (headPos - p + kHeadParts) % kHeadParts;
            headFFT->convolveAccumulate (ring + (size_t) idx * kHeadFFT, set->headSpectrum (r, o, p), accHead.get(), 1.0f);
            any = true;
        }
    }
    if (! any) return;
    headFFT->inverse (accHead.get(), tmpHead.get(), work.get());
    std::memcpy (dst, tmpHead.get() + kHeadBlock, (size_t) kHeadBlock * sizeof (float));
}

void MatrixConvolver::processBlock()
{
    if (blockInPeriod == 0)
        periodBoundary();

    // --- forward FFTs into the head FDLs, feed the tail input
    for (int i = 0; i < numFdl; ++i)
    {
        auto* blk = inBlock[(size_t) i].get();
        headFFT->forward (blk, headFdl[(size_t) i].get() + (size_t) headPos * kHeadFFT, work.get());
        if (i == 0)
            std::memcpy (tailIn.get() + kTailBlock + blockInPeriod * kHeadBlock, blk + kHeadBlock, (size_t) kHeadBlock * sizeof (float));
        std::memmove (blk, blk + kHeadBlock, (size_t) kHeadBlock * sizeof (float));
    }

    // --- head outputs (+ crossfade) and tail playback
    const float g0 = (float) blockInPeriod / (float) kBlocksPerPeriod;
    const float g1 = (float) (blockInPeriod + 1) / (float) kBlocksPerPeriod;
    const float gActive = gainFor (active), gFrom = gainFor (headFrom);

    for (int o = 0; o < numOuts; ++o)
    {
        auto& dst = outFifo[(size_t) o];
        computeHead (active, o, yA.data());
        if (headFading)
        {
            computeHead (headFrom, o, yB.data());
            for (int n = 0; n < kHeadBlock; ++n)
            {
                auto g = g0 + (g1 - g0) * (float) n / (float) kHeadBlock;
                dst[(size_t) n] = yA[(size_t) n] * gActive * g + yB[(size_t) n] * gFrom * (1.0f - g);
            }
        }
        else
        {
            for (int n = 0; n < kHeadBlock; ++n) dst[(size_t) n] = yA[(size_t) n] * gActive;
        }

        const float* tp = tailPlay[(size_t) o].get() + blockInPeriod * kHeadBlock;
        for (int n = 0; n < kHeadBlock; ++n) dst[(size_t) n] += tp[n];
    }

    headPos = (headPos + 1) % kHeadParts;

    // --- spread the tail job across the period
    const bool last = blockInPeriod == kBlocksPerPeriod - 1;
    const int budget = (job.totalUnits + kBlocksPerPeriod - 1) / kBlocksPerPeriod;
    runTailWork (budget, last);

    blockInPeriod = (blockInPeriod + 1) % kBlocksPerPeriod;
}

void MatrixConvolver::periodBoundary()
{
    // finish anything left from the previous period (should already be done)
    if (job.stage != 2) runTailWork (1 << 30, true);

    std::swap (tailPlay, tailCompute);

    // sets used only by the previous period can now go
    IRSet* prevHeadFrom = headFading ? headFrom : nullptr;
    IRSet* prevJobFrom = job.from;
    IRSet* prevJobTo = job.to;
    headFading = false;
    headFrom = nullptr;
    job.from = job.to = nullptr;

    // accept a new set
    if (auto* fresh = pending.exchange (nullptr))
    {
        bool ok = fresh->numOutputs == numOuts && fresh->tailParts <= maxParts;
        for (auto f : fresh->rowFdl) ok = ok && f >= 0 && f < numFdl;
        if (ok)
        {
            headFrom = active;
            headFading = true;
            active = fresh;
            activeSerial = fresh->serial;
        }
        else
        {
            retire (fresh);
        }
    }

    // schedule the tail job for the *next* period's output block
    job.to = active;
    job.from = headFading ? headFrom : nullptr;
    job.gTo = gainFor (job.to);
    job.gFrom = gainFor (job.from);
    job.stage = 0;
    job.out = job.slot = job.part = 0;
    job.doneUnits = 0;
    {
        int units = 6;
        for (int o = 0; o < numOuts; ++o)
        {
            if (job.to != nullptr)   units += job.to->tailPartsUsed[(size_t) o] + 6;
            if (job.from != nullptr) units += job.from->tailPartsUsed[(size_t) o] + 6;
        }
        job.totalUnits = units;
    }

    IRSet* candidates[3] = { prevHeadFrom, prevJobFrom, prevJobTo };
    for (int i = 0; i < 3; ++i)
    {
        bool dup = false;
        for (int j = 0; j < i; ++j) dup = dup || candidates[j] == candidates[i];
        if (! dup) retire (candidates[i]);
    }

    // tail input spectrum from [block q-2 | block q-1]; must run before this
    // period's first samples are written into tailIn
    tailPos = (tailPos + 1) % maxParts;
    tailFFT->forward (tailIn.get(), tailFdl.get() + (size_t) tailPos * kTailFFT, work.get());
    std::memmove (tailIn.get(), tailIn.get() + kTailBlock, (size_t) kTailBlock * sizeof (float));
    std::memset (tailIn.get() + kTailBlock, 0, (size_t) kTailBlock * sizeof (float));
    for (int o = 0; o < numOuts; ++o) { accTo[(size_t) o].clear(); accFrom[(size_t) o].clear(); }
    job.stage = 1;
    job.doneUnits = 6;
}

void MatrixConvolver::runTailWork (int budget, bool finish)
{
    while (job.stage != 2 && (finish || budget > 0))
    {

        // stage 1: per output, per slot (0 = to, 1 = from)
        if (job.out >= numOuts)
        {
            job.stage = 2;
            break;
        }

        IRSet* set = job.slot == 0 ? job.to : job.from;
        const int used = set != nullptr ? set->tailPartsUsed[(size_t) job.out] : 0;

        if (set == nullptr || job.part >= used)
        {
            // finalise this (out, slot)
            auto& acc = job.slot == 0 ? accTo[(size_t) job.out] : accFrom[(size_t) job.out];
            if (job.slot == 0)
            {
                auto* dst = tailCompute[(size_t) job.out].get();
                if (set != nullptr && used > 0)
                {
                    tailFFT->inverse (acc.get(), tmpTail.get(), work.get());
                    const float* y = tmpTail.get() + kTailBlock;
                    if (job.from != nullptr)
                        for (int n = 0; n < kTailBlock; ++n) dst[n] = y[n] * job.gTo * ((float) n / (float) kTailBlock);
                    else
                        for (int n = 0; n < kTailBlock; ++n) dst[n] = y[n] * job.gTo;
                    budget -= 6; job.doneUnits += 6;
                }
                else
                {
                    std::memset (dst, 0, (size_t) kTailBlock * sizeof (float));
                }
                if (job.from != nullptr) { job.slot = 1; job.part = 0; }
                else { job.out++; job.part = 0; }
            }
            else
            {
                if (set != nullptr && used > 0)
                {
                    tailFFT->inverse (acc.get(), tmpTail.get(), work.get());
                    const float* y = tmpTail.get() + kTailBlock;
                    auto* dst = tailCompute[(size_t) job.out].get();
                    for (int n = 0; n < kTailBlock; ++n) dst[n] += y[n] * job.gFrom * (1.0f - (float) n / (float) kTailBlock);
                    budget -= 6; job.doneUnits += 6;
                }
                job.slot = 0; job.out++; job.part = 0;
            }
            continue;
        }

        // one partition multiply-accumulate
        auto& acc = job.slot == 0 ? accTo[(size_t) job.out] : accFrom[(size_t) job.out];
        const int idx = (tailPos - job.part + maxParts) % maxParts;
        tailFFT->convolveAccumulate (tailFdl.get() + (size_t) idx * kTailFFT, set->tailSpectrum (job.out, job.part), acc.get(), 1.0f);
        job.part++;
        budget -= 1; job.doneUnits += 1;
    }
}

} // namespace srr
