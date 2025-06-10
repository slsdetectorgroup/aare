#pragma once

#include "aare/NDArray.hpp"
#include "aare/defs.hpp"

namespace aare {

NDArray<ssize_t, 2> GenerateMoench03PixelMap();
NDArray<ssize_t, 2> GenerateMoench05PixelMap();
NDArray<ssize_t, 2> GenerateMoench05PixelMap1g();
NDArray<ssize_t, 2> GenerateMoench05PixelMapOld();

// Matterhorn02
NDArray<ssize_t, 2> GenerateMH02SingleCounterPixelMap();
NDArray<ssize_t, 3> GenerateMH02FourCounterPixelMap();

// Eiger
NDArray<ssize_t, 2> GenerateEigerFlipRowsPixelMap();

} // namespace aare