// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace aare::test_cuda {

template <typename H> struct DeviceResult {
    NDArray<float, 1> centers, edges;
    NDArray<H, 3> values;
};

// Histogram `frames` (nframes, nrows, ncols) on the GPU, with frames of type
// In. Explicitly instantiated in the .cu for the storage and input types
// under test; the axis is float for all of them. `window` picks the fill
// kernel: fill_window (true) or fill_direct (false).
template <typename H, typename In>
DeviceResult<H> run_gpu(NDView<In, 3> frames, size_t nbins, float xmin,
                        float xmax, size_t batch_size, bool window = true);

// Fill one histogram from consecutive parts of `frames`, part i ending at
// frame ends[i], and take a values() snapshot after each part.
std::vector<NDArray<uint32_t, 3>>
run_gpu_in_parts(NDView<float, 3> frames, size_t nbins, float xmin, float xmax,
                 size_t batch_size, const std::vector<size_t> &ends,
                 bool window = true);

// Whether constructing a histogram with these arguments throws
// std::invalid_argument.
bool constructor_throws(size_t nbins, size_t batch_size, bool window);

// Known pattern written straight into the device accumulator, read back
// through values(); tests the layout and the cap without the fill kernel.
// Throws std::runtime_error describing the first mismatch.
void check_known_pattern();

struct FailedRegistration {
    bool registration_threw; // the second registration of one buffer
    bool fill_threw;         // the fill on a new histogram afterwards
    uint64_t counted;        // sum of that histogram's values()
    uint64_t expected;       // number of frame values, all in range
};

// Register one buffer in two histograms (CUDA refuses the second), then fill
// a third, new histogram. A CUDA error left over from the failed call must
// not make the fill throw after the work was queued.
FailedRegistration fill_after_failed_registration();

struct PixelQueries {
    NDArray<uint8_t, 3> values;
    std::vector<NDArray<uint8_t, 1>> pixels; // values_pixel(), one per query
    bool row_out_of_range_throws;            // values_pixel(nrows, 0)
    bool col_out_of_range_throws;            // values_pixel(0, ncols)
};

// Fill a uint8_t histogram from `frames`, then read values() and
// values_pixel() for each (row, col) in `pixels`.
PixelQueries
run_gpu_values_pixel(NDView<float, 3> frames, size_t nbins, float xmin,
                     float xmax, size_t batch_size,
                     const std::vector<std::pair<size_t, size_t>> &pixels);

} // namespace aare::test_cuda
