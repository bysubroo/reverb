#pragma once

#include <pffft.h>
#include <cstring>
#include <memory>
#include <utility>

namespace srr
{
// RAII wrapper around a pffft real-FFT setup (shared, read-only, thread safe)
class RealFFT
{
public:
    explicit RealFFT (int n) : size (n), setup (pffft_new_setup (n, PFFFT_REAL)) {}
    ~RealFFT() { if (setup != nullptr) pffft_destroy_setup (setup); }
    RealFFT (const RealFFT&) = delete;
    RealFFT& operator= (const RealFFT&) = delete;

    int getSize() const { return size; }
    bool isValid() const { return setup != nullptr; }

    // z-domain (internal order) transforms, used for fast convolution
    void forward (const float* in, float* out, float* work) const  { pffft_transform (setup, in, out, work, PFFFT_FORWARD); }
    void inverse (const float* in, float* out, float* work) const  { pffft_transform (setup, in, out, work, PFFFT_BACKWARD); }

    // canonical order: [DC, Nyquist, re1, im1, re2, im2 ...]
    void forwardOrdered (const float* in, float* out, float* work) const { pffft_transform_ordered (setup, in, out, work, PFFFT_FORWARD); }
    void inverseOrdered (const float* in, float* out, float* work) const { pffft_transform_ordered (setup, in, out, work, PFFFT_BACKWARD); }

    void convolveAccumulate (const float* a, const float* b, float* ab, float scale) const { pffft_zconvolve_accumulate (setup, a, b, ab, scale); }

    PFFFT_Setup* raw() const { return setup; }

private:
    int size;
    PFFFT_Setup* setup;
};

// 64-byte aligned float buffer
class AlignedBuffer
{
public:
    AlignedBuffer() = default;
    explicit AlignedBuffer (size_t n) { allocate (n); }
    ~AlignedBuffer() { release(); }
    AlignedBuffer (AlignedBuffer&& o) noexcept : data (std::exchange (o.data, nullptr)), count (std::exchange (o.count, 0)) {}
    AlignedBuffer& operator= (AlignedBuffer&& o) noexcept
    {
        if (this != &o) { release(); data = std::exchange (o.data, nullptr); count = std::exchange (o.count, 0); }
        return *this;
    }
    AlignedBuffer (const AlignedBuffer&) = delete;
    AlignedBuffer& operator= (const AlignedBuffer&) = delete;

    void allocate (size_t n)
    {
        release();
        if (n == 0) return;
        data = static_cast<float*> (pffft_aligned_malloc (n * sizeof (float)));
        count = n;
        clear();
    }
    void clear() { if (data != nullptr) std::memset (data, 0, count * sizeof (float)); }
    void release() { if (data != nullptr) pffft_aligned_free (data); data = nullptr; count = 0; }

    float* get() { return data; }
    const float* get() const { return data; }
    float& operator[] (size_t i) { return data[i]; }
    const float& operator[] (size_t i) const { return data[i]; }
    size_t size() const { return count; }

private:
    float* data = nullptr;
    size_t count = 0;
};

inline int nextPow2 (int v)
{
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}
} // namespace srr
