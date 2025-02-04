#include "aare/decode.hpp"

namespace aare {

uint16_t decode_adc(uint64_t input){

    //we want bits 29,19,28,18,31,21,27,20,24,23,25,22 and then pad to 16
    uint16_t output = 0;
    output |= ((input >> 29) & 1) << 15;
    output |= ((input >> 19) & 1) << 14;
    output |= ((input >> 28) & 1) << 13;
    output |= ((input >> 18) & 1) << 12;
    output |= ((input >> 31) & 1) << 11;
    output |= ((input >> 21) & 1) << 10;
    output |= ((input >> 27) & 1) << 9;
    output |= ((input >> 20) & 1) << 8;
    output |= ((input >> 24) & 1) << 7;
    output |= ((input >> 23) & 1) << 6;
    output |= ((input >> 25) & 1) << 5;
    output |= ((input >> 22) & 1) << 4;
    return output;
}

void decode_adc(NDView<uint64_t, 2> input, NDView<uint16_t,2> output){
    for(size_t i = 0; i < input.shape(0); i++){
        for(size_t j = 0; j < input.shape(1); j++){
            output(i,j) = decode_adc(input(i,j));
        }
    }
}

} // namespace aare