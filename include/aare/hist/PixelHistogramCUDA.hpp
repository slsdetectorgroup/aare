// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/hist/HistogramPipelineCUDA.hpp"
#include <cstdint>

#ifdef AARE_TESTS
#include "../tests/friend_test.hpp"
#endif

namespace aare::cuda {

/// Per-pixel histograms of a stack of frames, filled on the GPU.
///
/// Frames are copied to the GPU in batches of batch_size, as InputType, and
/// histogrammed there into one accumulator of DeviceStorageType counts that
/// stays on the GPU for the whole run. values() caps the counts at what
/// StorageType holds and copies them back, shape (rows, cols, n_bins).
///
/// Counts equal those of PixelHistogram<StorageType, AxisType> on the same
/// frames (AxisType: float for integer frames, the frame type otherwise).
/// Another axis type can bin a value that lies on a bin edge differently.
///
/// The copy of one batch overlaps the kernel on the previous one. With the
/// default window kernel the copy, not the kernel, sets the speed.
///
/// GPU memory: rows * cols * n_bins * sizeof(DeviceStorageType) for the
/// accumulator, 2 * batch_size * rows * cols * sizeof(InputType) for the frame
/// buffers, and during values() a temporary histogram of StorageType, unless
/// StorageType is DeviceStorageType.
///
/// Not thread-safe: one object must not be used from two threads at once.
///
/// The pipeline and every method but the constructor live in
/// detail::HistogramPipeline, shared with PedestalTrackingPixelHistogramCUDA.
///
/// @tparam StorageType type of the counts values() returns. Capped at its
///         maximum, like the CPU class.
/// @tparam DeviceStorageType unsigned integer type of the GPU counters. Use
///         uint64_t with 8-byte StorageType so counts can't wrap at 2^32.
/// @tparam InputType type of the frames: floating point, or an integer of at
///         most 16 bits.
/// @tparam BLOCK_SIZE threads per block of the fill kernels.
template <typename StorageType = uint16_t,
          typename DeviceStorageType = uint32_t, typename InputType = float,
          int BLOCK_SIZE = 128>
class PixelHistogramCUDA
    : public detail::HistogramPipeline<
          detail::PlainFill<InputType, detail::plain_axis_t<InputType>>,
          StorageType, DeviceStorageType, BLOCK_SIZE> {
#ifdef AARE_TESTS
    // Writes a known pattern into d_histogram to test values() without the
    // fill kernel (PixelHistogramCUDA_testimpl.cu).
    FRIEND_TEST(known_pattern_cap)
#endif
    using Base = detail::HistogramPipeline<
        detail::PlainFill<InputType, detail::plain_axis_t<InputType>>,
        StorageType, DeviceStorageType, BLOCK_SIZE>;

  public:
    /// Type of the range, the bin edges and the bin arithmetic: InputType
    /// if floating point, float otherwise.
    using AxisType = typename Base::AxisType;

    /// n_bins equal bins over [xmin, xmax); values outside, and NaN, are not
    /// counted. Throws std::invalid_argument for zero rows, cols, n_bins or
    /// batch_size, a non-finite range or xmax <= xmin.
    ///
    /// @param batch_size frames copied and histogrammed per step. Two batches
    ///        are held on the GPU (see the class description): lower it for
    ///        large frames. With the default 1024 the kernel needs under half
    ///        the copy time, so the copy sets the speed.
    /// @param window true: fill_window counts each pixel's 128 bins around its
    ///        noise peak in shared memory (needs batch_size <= 65535). false:
    ///        fill_direct, every value straight to GPU memory. Both give
    ///        identical counts; window is faster for fine bins.
    PixelHistogramCUDA(size_t rows, size_t cols, size_t n_bins, AxisType xmin,
                       AxisType xmax, size_t batch_size = 1024,
                       bool window = true)
        : Base("PixelHistogramCUDA", rows, cols, n_bins, xmin, xmax, batch_size,
               window) {}
};

} // namespace aare::cuda
