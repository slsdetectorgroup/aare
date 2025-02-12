#include "aare/decode.hpp"

namespace aare {

uint16_t adc_sar_05_decode64to16(uint64_t input){

    //we want bits 29,19,28,18,31,21,27,20,24,23,25,22 and then pad to 16
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

void adc_sar_05_decode64to16(NDView<uint64_t, 2> input, NDView<uint16_t,2> output){
    for(int64_t i = 0; i < input.shape(0); i++){
        for(int64_t j = 0; j < input.shape(1); j++){
            output(i,j) = adc_sar_05_decode64to16(input(i,j));
        }
    }
}

uint16_t adc_sar_04_decode64to16(uint64_t input){

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

void adc_sar_04_decode64to16(NDView<uint64_t, 2> input, NDView<uint16_t,2> output){
    for(int64_t i = 0; i < input.shape(0); i++){
        for(int64_t j = 0; j < input.shape(1); j++){
            output(i,j) = adc_sar_04_decode64to16(input(i,j));
        }
    }
}



} // namespace aare
