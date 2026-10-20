// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include <cstddef>
#include <cstdint>

namespace aare::test_cuda {

template <typename H> struct DeviceResult {
    NDArray<float, 1> centers, edges;
    NDArray<H, 3> values;
};

// Histogram `frames` (nframes, nrows, ncols) on the GPU. Explicitly
// instantiated in the .cu for the storage types under test.
template <typename H>
DeviceResult<H> run_gpu(NDView<float, 3> frames, size_t nbins, float xmin,
                        float xmax, size_t batch_size);

} // namespace aare::test_cuda
