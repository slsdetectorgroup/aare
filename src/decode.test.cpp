// SPDX-License-Identifier: MPL-2.0
#include "aare/decode.hpp"

#include "aare/NDArray.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
using Catch::Matchers::WithinAbs;
#include <vector>

using aare::BitOffset;

TEST_CASE("test_adc_sar_05_decode64to16") {
    uint64_t input = 0;
    uint16_t output = aare::adc_sar_05_decode64to16(input);
    CHECK(output == 0);

    // bit 29 on th input is bit 0 on the output
    input = 1UL << 29;
    output = aare::adc_sar_05_decode64to16(input);
    CHECK(output == 1);

    // test all bits by iteratting through the bitlist
    std::vector<int> bitlist = {29, 19, 28, 18, 31, 21, 27, 20, 24, 23, 25, 22};
    for (size_t i = 0; i < bitlist.size(); i++) {
        input = 1UL << bitlist[i];
        output = aare::adc_sar_05_decode64to16(input);
        CHECK(output == (1 << i));
    }

    // test a few "random" values
    input = 0;
    input |= (1UL << 29);
    input |= (1UL << 19);
    input |= (1UL << 28);
    output = aare::adc_sar_05_decode64to16(input);
    CHECK(output == 7UL);

    input = 0;
    input |= (1UL << 18);
    input |= (1UL << 27);
    input |= (1UL << 25);
    output = aare::adc_sar_05_decode64to16(input);
    CHECK(output == 1096UL);

    input = 0;
    input |= (1UL << 25);
    input |= (1UL << 22);
    output = aare::adc_sar_05_decode64to16(input);
    CHECK(output == 3072UL);
}

TEST_CASE("test_apply_custom_weights") {

    uint16_t input = 1;
    aare::NDArray<double, 1> weights_data({3}, 0.0);
    weights_data(0) = 1.7;
    weights_data(1) = 2.1;
    weights_data(2) = 1.8;

    auto weights = weights_data.view();

    double output = aare::apply_custom_weights(input, weights);
    CHECK_THAT(output, WithinAbs(1.0, 0.001));

    input = 1 << 1;
    output = aare::apply_custom_weights(input, weights);
    CHECK_THAT(output, WithinAbs(2.1, 0.001));

    input = 1 << 2;
    output = aare::apply_custom_weights(input, weights);
    CHECK_THAT(output, WithinAbs(3.24, 0.001));

    input = 0b111;
    output = aare::apply_custom_weights(input, weights);
    CHECK_THAT(output, WithinAbs(6.34, 0.001));
}

TEST_CASE("Mask 32 bit unsigned integer to 24 bit"){
    //any number less than 2**24 (16777216) should be the same
    // CHECK(aare::mask32to24bits(0)==0);
    // CHECK(aare::mask32to24bits(19)==19);
    // CHECK(aare::mask32to24bits(29875)==29875);
    // CHECK(aare::mask32to24bits(1092177)==1092177);
    // CHECK(aare::mask32to24bits(0xFFFF)==0xFFFF);
    // CHECK(aare::mask32to24bits(0xFFFFFFFF)==0xFFFFFF);

    //Offset specifies that the should ignore 0-7 bits
    //at the start
    // CHECK(aare::mask32to24bits(0xFFFF, BitOffset(4))==0xFFF);
    // CHECK(aare::mask32to24bits(0xFF0000d9)==0xd9);
    // CHECK(aare::mask32to24bits(0xFF000d9F, BitOffset(4))==0xF000d9);
    // CHECK(aare::mask32to24bits(16777217)==1);
    // CHECK(aare::mask32to24bits(15,BitOffset(7))==0);
    
    // //Highest bit set to 1 should just be excluded
    // //lowest 4 bits set to 1 
    // CHECK(aare::mask32to24bits(0x8000000f,BitOffset(7))==0);
    
}

// TEST_CASE("Expand container with 24 bit data to 32"){
//     {
//         uint8_t buffer[] = {
//             0x00, 0x00, 0x00, 
//             0x00, 0x00, 0x00,
//             0x00, 0x00, 0x00,
//         };

//         aare::NDView<uint8_t, 1> input(&buffer[0], {9});
//         aare::NDArray<uint32_t, 1> out({3});
//         aare::expand24to32bit(input, out.view());

//         CHECK(out(0) == 0);
//         CHECK(out(1) == 0);
//         CHECK(out(2) == 0);
//     }
//     {
//         uint8_t buffer[] = {
//             0x0F, 0x00, 0x00, 
//             0xFF, 0x00, 0x00,
//             0xFF, 0xFF, 0xFF,
//         };

//         aare::NDView<uint8_t, 1> input(&buffer[0], {9});
//         aare::NDArray<uint32_t, 1> out({3});
//         aare::expand24to32bit(input, out.view());

//         CHECK(out(0) == 0xF);
//         CHECK(out(1) == 0xFF);
//         CHECK(out(2) == 0xFFFFFF);
//     }
//     {
//         uint8_t buffer[] = {
//             0x00, 0x00, 0xFF, 
//             0xFF, 0xFF, 0x00,
//             0x00, 0xFF, 0x00,
//         };

//         aare::NDView<uint8_t, 1> input(&buffer[0], {9});
//         aare::NDArray<uint32_t, 1> out({3});
//         aare::expand24to32bit(input, out.view());

//         CHECK(out(0) == 0xFF0000);
//         CHECK(out(1) == 0xFFFF);
//         CHECK(out(2) == 0xFF00);
//     }
//     {
//         //For use with offset we need an extra byte
//         uint8_t buffer[] = {
//             0x00, 0x00, 0xFF, 
//             0xFF, 0xFF, 0x00,
//             0x00, 0xFF, 0x00, 0x00
//         };

//         aare::NDView<uint8_t, 1> input(&buffer[0], {10});
//         aare::NDArray<uint32_t, 1> out({3}); //still output.size == 3
//         aare::expand24to32bit(input, out.view(), BitOffset(4));

//         CHECK(out(0) == 0xFFF000);
//         CHECK(out(1) == 0xFFF);
//         CHECK(out(2) == 0xFF0);
//     }

// }