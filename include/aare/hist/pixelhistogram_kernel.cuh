// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <cstddef>
#include <cstdint>
#include <cuda_runtime.h>

namespace aare::cuda::detail {

/// Width, in bins, of each pixel's shared-memory window in fill_window.
/// Chosen with perf/hist/window_hits.py on MÖNCH frames (noise sigma ~21
/// ADU): 128 bins cover +-3 sigma down to 1 ADU per bin.
inline constexpr unsigned WINDOW_BINS = 128;

/// Index of (pixel, bin) in the device accumulator. The accumulator is
/// pixel-major, [npix][nbins], the layout values() returns.
__device__ __forceinline__ size_t hist_index(size_t pix_id, size_t bin_id,
                                             size_t nbins) {
    return pix_id * nbins + bin_id;
}

/// Bin of one value, as PixelHistogramImpl::fill_flat_unchecked computes it.
/// Returns false for values outside [xmin, xmax), NaN included.
template <typename InputType, typename AxisType>
__device__ __forceinline__ bool bin_of(InputType raw, AxisType xmin,
                                       AxisType xmax, AxisType scale,
                                       size_t nbins, size_t &bin_id) {
    const AxisType val = static_cast<AxisType>(raw);
    if (!(val >= xmin && val < xmax)) // also skips NaN
        return false;
    bin_id = min(static_cast<size_t>((val - xmin) * scale), nbins - 1);
    return true;
}

// Copies the device accumulator to d_out, capping each count at
// saturation_value before narrowing it to StorageType. Same layout in and
// out. One thread per count.
template <typename StorageType, typename DeviceStorageType>
__global__ void cap_counts(const DeviceStorageType *__restrict__ d_counts,
                           StorageType *__restrict__ d_out, const size_t n,
                           DeviceStorageType saturation_value) {
    // Widen before multiplying: blockIdx.x * blockDim.x is computed in 32 bits
    // otherwise and wraps once the grid covers more than 2^32 elements.
    const size_t g_tid =
        static_cast<size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (g_tid >= n)
        return;

    const DeviceStorageType count = d_counts[g_tid];
    d_out[g_tid] = static_cast<StorageType>(
        count < saturation_value ? count : saturation_value);
}

// Frames arrive as InputType; each value is converted to the floating-point
// AxisType before binning.
template <typename DeviceStorageType, typename InputType, typename AxisType>
__global__ void fill_direct(const InputType *__restrict__ d_frames_batch,
                            DeviceStorageType *__restrict__ d_histogram,
                            const size_t npix, const size_t nbins,
                            AxisType xmin, AxisType xmax, AxisType scale,
                            size_t batch_size) {
    size_t pix_id = static_cast<size_t>(blockIdx.x) * blockDim.x + threadIdx.x;

    if (pix_id >= npix)
        return;

    for (size_t fid = 0; fid < batch_size; ++fid) {
        size_t bin_id;
        if (bin_of(d_frames_batch[fid * npix + pix_id], xmin, xmax, scale,
                   nbins, bin_id))
            ++d_histogram[hist_index(pix_id, bin_id, nbins)];
    }
}

// Same counts as fill_direct, with less global-memory traffic. Each thread
// keeps a window of WINDOW_BINS consecutive bins of its pixel's histogram in
// shared memory for the batch: values inside it are counted there, all
// others go to global memory as in fill_direct. At the end of the batch
// every non-zero window counter is added to its global bin. The result is
// exact wherever the window sits; its placement only affects speed.
//
// The window follows the pixel's noise peak through d_centre[pix_id], in
// bin units, NaN until placed:
//   1. Not placed: mean bin of the batch's in-range values, then the mean of
//      those within WINDOW_BINS / 2 of it (drops the photons).
//   2. After the flush: the mean bin of the window counts, so the window
//      walks onto the peak and follows drift. Photons never enter it.
//   3. Window caught under half of the in-range values: back to NaN, so
//      step 1 runs again next batch.
//
// Launch with WINDOW_BINS * blockDim.x * sizeof(uint16_t) bytes of dynamic
// shared memory. The uint16_t counters need batch_size <= 65535.
template <typename DeviceStorageType, typename InputType, typename AxisType>
__global__ void fill_window(const InputType *__restrict__ d_frames_batch,
                            DeviceStorageType *__restrict__ d_histogram,
                            float *__restrict__ d_centre, const size_t npix,
                            const size_t nbins, AxisType xmin, AxisType xmax,
                            AxisType scale, size_t batch_size) {
    constexpr unsigned W = WINDOW_BINS;
    // [W][blockDim.x]: a warp touches 32 consecutive counters, no bank
    // conflicts. Each thread only uses its own column: no __syncthreads().
    extern __shared__ uint16_t win[];
    const unsigned tid = threadIdx.x;
    const unsigned stride = blockDim.x;

    const size_t pix_id =
        static_cast<size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (pix_id >= npix)
        return;

    // ---- 1. place the window if needed -------------------------------------
    float centre = d_centre[pix_id];
    if (isnan(centre)) {
        unsigned long long sum = 0;
        unsigned n = 0;
        size_t bin_id;
        for (size_t fid = 0; fid < batch_size; ++fid) {
            if (bin_of(d_frames_batch[fid * npix + pix_id], xmin, xmax, scale,
                       nbins, bin_id)) {
                sum += bin_id;
                ++n;
            }
        }
        if (n == 0) {
            centre = 0.5f * static_cast<float>(nbins); // nothing to go by
        } else {
            const float mean = static_cast<float>(sum) / n;
            sum = 0;
            unsigned n_near = 0;
            for (size_t fid = 0; fid < batch_size; ++fid) {
                if (bin_of(d_frames_batch[fid * npix + pix_id], xmin, xmax,
                           scale, nbins, bin_id) &&
                    fabsf(static_cast<float>(bin_id) - mean) < 0.5f * W) {
                    sum += bin_id;
                    ++n_near;
                }
            }
            centre = n_near ? static_cast<float>(sum) / n_near : mean;
        }
    }

    // Window [start, start + W), kept inside [0, nbins).
    const long long max_start =
        nbins > W ? static_cast<long long>(nbins - W) : 0;
    long long first = static_cast<long long>(centre) - W / 2;
    first = first < 0 ? 0 : (first > max_start ? max_start : first);
    const size_t start = static_cast<size_t>(first);

    // ---- fill --------------------------------------------------------------
    for (unsigned k = 0; k < W; ++k)
        win[k * stride + tid] = 0;

    unsigned n_in = 0;
    for (size_t fid = 0; fid < batch_size; ++fid) {
        size_t bin_id;
        if (!bin_of(d_frames_batch[fid * npix + pix_id], xmin, xmax, scale,
                    nbins, bin_id))
            continue;
        ++n_in;
        const size_t k = bin_id - start; // wraps below start: one compare
        if (k < W)
            ++win[k * stride + tid];
        else
            ++d_histogram[hist_index(pix_id, bin_id, nbins)];
    }

    // ---- flush, then 2. move the window ------------------------------------
    unsigned hits = 0;
    unsigned long long weighted = 0;
    for (unsigned k = 0; k < W; ++k) {
        const unsigned count = win[k * stride + tid];
        if (count) {
            d_histogram[hist_index(pix_id, start + k, nbins)] += count;
            hits += count;
            weighted += static_cast<unsigned long long>(count) * k;
        }
    }

    // ---- 3. centre for the next batch --------------------------------------
    if (n_in == 0)
        d_centre[pix_id] = centre; // nothing new: keep it
    else if (2 * hits < n_in)
        d_centre[pix_id] = __int_as_float(0x7fffffff); // NaN: place again
    else
        d_centre[pix_id] =
            static_cast<float>(start) + static_cast<float>(weighted) / hits;
}

} // namespace aare::cuda::detail
