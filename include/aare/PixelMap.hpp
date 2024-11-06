#pragma once

#include "aare/defs.hpp"
#include "aare/NDArray.hpp"

namespace aare {

NDArray<ssize_t, 2> GenerateMoench03PixelMap();
NDArray<ssize_t, 2> GenerateMoench05PixelMap();

NDArray<ssize_t, 2>GenerateMH02SingleCounterPixelMap();
NDArray<ssize_t, 3> GenerateMH02FourCounterPixelMap();


} // namespace aare