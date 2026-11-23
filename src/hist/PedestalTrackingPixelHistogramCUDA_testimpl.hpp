// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace aare::test_cuda {

struct TrackingSetup {
    size_t nbins = 300;
    float xmin = -50.0f;
    float xmax = 250.0f;
    float n_sigma = 1.0f;
    const NDArray<bool, 2> *mask = nullptr; // nullptr: no mask
    uint32_t n_samples = 1000;
    size_t batch_size = 64;
    bool window = true;
};

// State after one part of the fill.
struct TrackingSnapshot {
    NDArray<uint16_t, 3> values;
    NDArray<float, 2> mean;
    NDArray<float, 2> std;
};

// Runs the sequence the tests compare with the CPU class:
//   fill `before` (may hold 0 frames), push every frame of `seed`,
//   update_mean(), then fill `data` in parts ending at part_ends, taking a
//   snapshot after each part and calling update_mean() after each part when
//   update_between is set.
std::vector<TrackingSnapshot>
run_tracking_gpu(NDView<uint16_t, 3> before, NDView<uint16_t, 3> seed,
                 NDView<uint16_t, 3> data, const TrackingSetup &setup,
                 const std::vector<size_t> &part_ends, bool update_between);

struct TrackingErrors {
    bool update_mean_before_seed;   // std::runtime_error
    bool pedestal_mean_before_seed; // std::runtime_error
    bool push_after_seed;           // std::runtime_error
    bool push_wrong_shape;          // std::invalid_argument
    bool zero_n_samples;            // std::invalid_argument
    bool mask_wrong_shape;          // std::invalid_argument
    bool std_zero_before_update;    // pedestal_std() all 0 before update_mean
    uint32_t frames_after_seed;     // pedestal_frames() once seeded
};

// Which misuses throw what; n_samples = 3, 2x3 pixels.
TrackingErrors tracking_errors();

} // namespace aare::test_cuda
