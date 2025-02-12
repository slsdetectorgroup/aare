#pragma once

#include <cstdint>
#include <aare/NDView.hpp>
namespace aare {


uint16_t adc_sar_05_decode64to16(uint64_t input);
uint16_t adc_sar_04_decode64to16(uint64_t input);
void adc_sar_05_decode64to16(NDView<uint64_t, 2> input, NDView<uint16_t,2> output);
void adc_sar_04_decode64to16(NDView<uint64_t, 2> input, NDView<uint16_t,2> output);

} // namespace aare