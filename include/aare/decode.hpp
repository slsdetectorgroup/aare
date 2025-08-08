#pragma once

#include <aare/NDView.hpp>
#include <cstdint>
#include <vector>
namespace aare {

uint16_t adc_sar_05_decode64to16(uint64_t input);
uint16_t adc_sar_04_decode64to16(uint64_t input);
void adc_sar_05_decode64to16(NDView<uint64_t, 2> input,
                             NDView<uint16_t, 2> output);
void adc_sar_04_decode64to16(NDView<uint64_t, 2> input,
                             NDView<uint16_t, 2> output);

/**
 * @brief Apply custom weights to a 16-bit input value. Will sum up
 * weights[i]**i for each bit i that is set in the input value.
 * @throws std::out_of_range if weights.size() < 16
 * @param input 16-bit input value
 * @param weights vector of weights, size must be less than or equal to 16
 */
double apply_custom_weights(uint16_t input, const NDView<double, 1> weights);

void apply_custom_weights(NDView<uint16_t, 1> input, NDView<double, 1> output,
                          const NDView<double, 1> weights);

} // namespace aare
