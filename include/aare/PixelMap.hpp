// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "aare/NDArray.hpp"
#include "aare/defs.hpp"

namespace aare {

NDArray<ssize_t, 2> GenerateMoench03PixelMap();
NDArray<ssize_t, 2> GenerateMoench05PixelMap();
NDArray<ssize_t, 2> GenerateMoench05PixelMap1g();
NDArray<ssize_t, 2> GenerateMoench05PixelMapOld();
NDArray<ssize_t, 2> GenerateMoench04AnalogPixelMap();

// Matterhorn02
NDArray<ssize_t, 2> GenerateMH02SingleCounterPixelMap();
NDArray<ssize_t, 3> GenerateMH02FourCounterPixelMap();

/**
 * @brief Generate pixel map for Matterhorn10 detector
 * @param dynamic_range Dynamic range of the detector (16, 8, or 4)
 * @param n_counters Number of counters (1 to 4)
 */
NDArray<ssize_t, 2>
GenerateMatterhorn10PixelMap(const size_t dynamic_range = 16,
                             const size_t n_counters = 1);

// Eiger
NDArray<ssize_t, 2> GenerateEigerFlipRowsPixelMap();

} // namespace aare