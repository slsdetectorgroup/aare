// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/defs.hpp"
#include <aare/NDView.hpp>
#include <cstdint>
#include <vector>
namespace aare {

uint16_t adc_sar_05_06_07_08decode64to16(uint64_t input);
uint16_t adc_sar_05_decode64to16(uint64_t input);
uint16_t adc_sar_04_decode64to16(uint64_t input);
void adc_sar_05_06_07_08decode64to16(NDView<uint64_t, 2> input,
                                     NDView<uint16_t, 2> output);
void adc_sar_05_decode64to16(NDView<uint64_t, 2> input,
                             NDView<uint16_t, 2> output);
void adc_sar_04_decode64to16(NDView<uint64_t, 2> input,
                             NDView<uint16_t, 2> output);

/**
 * @brief Decode packed 64-bit ADC SAR samples stored as raw bytes.
 *
 * Every 8 bytes of a row form one 64-bit word in native byte order. Words
 * are assembled with memcpy, so the buffer does not need to be 8-byte
 * aligned. The output must have the same number of rows as the input and
 * input.shape(1) / 8 columns.
 * @throws std::invalid_argument if the row length is not a multiple of 8
 * bytes or the output shape does not match.
 */
void adc_sar_05_06_07_08decode64to16(NDView<const uint8_t, 2> input,
                                     NDView<uint16_t, 2> output);
void adc_sar_05_decode64to16(NDView<const uint8_t, 2> input,
                             NDView<uint16_t, 2> output);
void adc_sar_04_decode64to16(NDView<const uint8_t, 2> input,
                             NDView<uint16_t, 2> output);

/**
 * @brief Called with a 32 bit unsigned integer, shift by offset
 * and then return the lower 24 bits as an 32 bit integer
 * @param input 32-ibt input value
 * @param offset (should be in range 0-7 to allow for full 24 bits)
 * @return uint32_t
 */
uint32_t mask32to24bits(uint32_t input, BitOffset offset = {});

/**
 * @brief Expand 24 bit values in a 8bit buffer to 32bit unsigned integers
 * Used for detectors with 24bit counters in combination with CTB
 *
 * @param input View of the 24 bit data as uint8_t (no 24bit native data type
 * exists)
 * @param output Destination of the expanded data (32bit, unsigned)
 * @param offset Offset within the first byte to where the data starts (0-7
 * bits)
 */
void expand24to32bit(NDView<const uint8_t, 1> input, NDView<uint32_t, 1> output,
                     BitOffset offset = {});

/**
 * @brief expands the two 4 bit values of an 8 bit buffer into two 8 bit values
 * @param input input buffer with 4 bit values packed into 8 bit
 * @param output output buffer with 8 bit values
 */
void expand4to8bit(NDView<const uint8_t, 1> input, NDView<uint8_t, 1> output);

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
