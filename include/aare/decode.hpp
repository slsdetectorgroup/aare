#pragma once

#include <cstdint>
#include <aare/NDView.hpp>
namespace aare {


uint16_t decode_adc(uint64_t input);

void decode_adc(NDView<uint64_t, 2> input, NDView<uint16_t,2> output);

} // namespace aare