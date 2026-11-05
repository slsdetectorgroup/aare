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
// under test; the axis is float for all of them.
template <typename H, typename In>
DeviceResult<H> run_gpu(NDView<In, 3> frames, size_t nbins, float xmin,
                        float xmax, size_t batch_size);

// Fill one histogram from consecutive parts of `frames`, part i ending at
// frame ends[i], and take a values() snapshot after each part.
std::vector<NDArray<uint32_t, 3>>
run_gpu_in_parts(NDView<float, 3> frames, size_t nbins, float xmin, float xmax,
                 size_t batch_size, const std::vector<size_t> &ends);

// Known pattern written straight into the device accumulator, read back
// through values(); tests the transpose and the cap without the fill kernel.
// Throws std::runtime_error describing the first mismatch.
void check_known_pattern();

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
