// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <cstddef>
#include <cstdint>
#include <cuda_runtime.h>

namespace aare::cuda::detail {

/// Accumulator layout. false: bin-major [nbins][npix]. true: pixel-major
/// [npix][nbins], the layout values() returns, so readout only caps.
/// Experiment switch: rebuild to change it.
inline constexpr bool PIXEL_MAJOR = false;

// Transposes the [nbins][npix] device histogram into [npix][nbins] and
// saturates each count at saturation_value before narrowing to StorageType.
// One thread per output element: writes are coalesced, reads are strided.
template <typename StorageType, typename DeviceStorageType>
__global__ void
bin_to_pixel_major(const DeviceStorageType *__restrict__ d_histogram_binMajor,
                   StorageType *__restrict__ d_histogram_pixMajor,
                   const size_t npix, const size_t nbins,
                   DeviceStorageType saturation_value) {
    // Widen before multiplying: blockIdx.x * blockDim.x is computed in 32 bits
    // otherwise and wraps once the grid covers more than 2^32 elements.
    const size_t g_tid =
        static_cast<size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (g_tid >= npix * nbins)
        return;

    // g_tid = pix_id * nbins + bin_id; one division, the remainder by
    // multiply-subtract.
    const size_t pix_id = g_tid / nbins;
    const size_t bin_id = g_tid - pix_id * nbins;

    const DeviceStorageType count =
        PIXEL_MAJOR ? d_histogram_binMajor[g_tid]
                    : d_histogram_binMajor[bin_id * npix + pix_id];

    d_histogram_pixMajor[g_tid] = static_cast<StorageType>(
        count < saturation_value ? count : saturation_value);
}

// Frames arrive as InputType; each value is converted to the floating-point
// AxisType before binning.
template <typename DeviceStorageType, typename InputType, typename AxisType>
__global__ void fill_bin_major(const InputType *__restrict__ d_frames_batch,
                               DeviceStorageType *__restrict__ d_histogram,
                               const size_t npix, const size_t nbins,
                               AxisType xmin, AxisType xmax, AxisType scale,
                               size_t batch_size) {
    size_t pix_id = static_cast<size_t>(blockIdx.x) * blockDim.x + threadIdx.x;

    if (pix_id >= npix)
        return;

    for (size_t fid = 0; fid < batch_size; ++fid) {

        const AxisType val_pix_at_fid =
            static_cast<AxisType>(d_frames_batch[fid * npix + pix_id]);
        if (!(val_pix_at_fid >= xmin &&
              val_pix_at_fid < xmax)) // also skips NaN
            continue;

        size_t bin_id = min(
            static_cast<size_t>((val_pix_at_fid - xmin) * scale), nbins - 1);
        if constexpr (PIXEL_MAJOR)
            ++d_histogram[pix_id * nbins + bin_id];
        else
            ++d_histogram[pix_id + npix * bin_id];
    }
}

} // namespace aare::cuda::detail
