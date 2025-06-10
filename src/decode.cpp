#include "aare/decode.hpp"
#include <cmath>
namespace aare {

uint16_t adc_sar_05_decode64to16(uint64_t input) {

    // we want bits 29,19,28,18,31,21,27,20,24,23,25,22 and then pad to 16
    uint16_t output = 0;
    output |= ((input >> 22) & 1) << 11;
    output |= ((input >> 25) & 1) << 10;
    output |= ((input >> 23) & 1) << 9;
    output |= ((input >> 24) & 1) << 8;
    output |= ((input >> 20) & 1) << 7;
    output |= ((input >> 27) & 1) << 6;
    output |= ((input >> 21) & 1) << 5;
    output |= ((input >> 31) & 1) << 4;
    output |= ((input >> 18) & 1) << 3;
    output |= ((input >> 28) & 1) << 2;
    output |= ((input >> 19) & 1) << 1;
    output |= ((input >> 29) & 1) << 0;
    return output;
}

void adc_sar_05_decode64to16(NDView<uint64_t, 2> input,
                             NDView<uint16_t, 2> output) {
    if (input.shape() != output.shape()) {
        throw std::invalid_argument(LOCATION +
                                    " input and output shapes must match");
    }

    for (ssize_t i = 0; i < input.shape(0); i++) {
        for (ssize_t j = 0; j < input.shape(1); j++) {
            output(i, j) = adc_sar_05_decode64to16(input(i, j));
        }
    }
}

uint16_t adc_sar_04_decode64to16(uint64_t input) {

    // bit_map = array([15,17,19,21,23,4,6,8,10,12,14,16] LSB->MSB
    uint16_t output = 0;
    output |= ((input >> 16) & 1) << 11;
    output |= ((input >> 14) & 1) << 10;
    output |= ((input >> 12) & 1) << 9;
    output |= ((input >> 10) & 1) << 8;
    output |= ((input >> 8) & 1) << 7;
    output |= ((input >> 6) & 1) << 6;
    output |= ((input >> 4) & 1) << 5;
    output |= ((input >> 23) & 1) << 4;
    output |= ((input >> 21) & 1) << 3;
    output |= ((input >> 19) & 1) << 2;
    output |= ((input >> 17) & 1) << 1;
    output |= ((input >> 15) & 1) << 0;
    return output;
}

void adc_sar_04_decode64to16(NDView<uint64_t, 2> input,
                             NDView<uint16_t, 2> output) {
    if (input.shape() != output.shape()) {
        throw std::invalid_argument(LOCATION +
                                    " input and output shapes must match");
    }
    for (ssize_t i = 0; i < input.shape(0); i++) {
        for (ssize_t j = 0; j < input.shape(1); j++) {
            output(i, j) = adc_sar_04_decode64to16(input(i, j));
        }
    }
}

double apply_custom_weights(uint16_t input, const NDView<double, 1> weights) {
    if (weights.size() > 16) {
        throw std::invalid_argument(
            "weights size must be less than or equal to 16");
    }

    double result = 0.0;
    for (ssize_t i = 0; i < weights.size(); ++i) {
        result += ((input >> i) & 1) * std::pow(weights[i], i);
    }
    return result;
}

void apply_custom_weights(NDView<uint16_t, 1> input, NDView<double, 1> output,
                          const NDView<double, 1> weights) {
    if (input.shape() != output.shape()) {
        throw std::invalid_argument(LOCATION +
                                    " input and output shapes must match");
    }

    // Calculate weights to avoid repeatedly calling std::pow
    std::vector<double> weights_powers(weights.size());
    for (ssize_t i = 0; i < weights.size(); ++i) {
        weights_powers[i] = std::pow(weights[i], i);
    }

    // Apply custom weights to each element in the input array
    for (ssize_t i = 0; i < input.shape(0); i++) {
        double result = 0.0;
        for (size_t bit_index = 0; bit_index < weights_powers.size();
             ++bit_index) {
            result += ((input(i) >> bit_index) & 1) * weights_powers[bit_index];
        }
        output(i) = result;
    }
}

} // namespace aare
