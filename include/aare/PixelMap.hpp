#pragma once

#include "aare/defs.hpp"
#include "aare/NDArray.hpp"

namespace aare {

NDArray<ssize_t, 2> GenerateMoench03PixelMap();
NDArray<ssize_t, 2> GenerateMoench05PixelMap();

//Matterhorn02
NDArray<ssize_t, 2>GenerateMH02SingleCounterPixelMap();
NDArray<ssize_t, 3> GenerateMH02FourCounterPixelMap();

//Eiger 
NDArray<ssize_t, 2>GenerateEigerFlipRowsPixelMap();

} // namespace aare