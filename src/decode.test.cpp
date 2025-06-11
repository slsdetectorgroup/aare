#include "aare/decode.hpp"

#include "aare/NDArray.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
using Catch::Matchers::WithinAbs;
#include <vector>

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