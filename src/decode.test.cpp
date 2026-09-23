// SPDX-License-Identifier: MPL-2.0
#include "aare/decode.hpp"

#include "aare/NDArray.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
using Catch::Matchers::WithinAbs;
#include <cstring>
#include <stdexcept>
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

TEST_CASE("Decode 64 bit ADC SAR samples from a byte buffer") {
    // Two rows of two words each, viewed as raw bytes
    aare::NDArray<uint64_t, 2> words({2, 2});
    words(0, 0) = 1UL << 29;
    words(0, 1) = (1UL << 25) | (1UL << 22);
    words(1, 0) = 0;
    words(1, 1) = (1UL << 29) | (1UL << 19) | (1UL << 28);

    aare::NDView<const uint8_t, 2> bytes(
        reinterpret_cast<const uint8_t *>(words.data()), {2, 16});
    aare::NDArray<uint16_t, 2> out({2, 2});
    aare::adc_sar_05_decode64to16(bytes, out.view());

    CHECK(out(0, 0) == 1);
    CHECK(out(0, 1) == 3072);
    CHECK(out(1, 0) == 0);
    CHECK(out(1, 1) == 7);

    // Matches the overload that takes the words directly
    aare::NDArray<uint16_t, 2> reference({2, 2});
    aare::adc_sar_05_decode64to16(words.view(), reference.view());
    for (ssize_t i = 0; i < 2; ++i) {
        for (ssize_t j = 0; j < 2; ++j) {
            CHECK(out(i, j) == reference(i, j));
        }
    }
}

TEST_CASE("Each byte buffer decoder maps to its own bit order") {
    // First two bits of each decoder's bit list
    struct Case {
        void (*decode)(aare::NDView<const uint8_t, 2>,
                       aare::NDView<uint16_t, 2>);
        int bit0;
        int bit1;
    };
    const Case cases[] = {
        {aare::adc_sar_05_06_07_08decode64to16, 29, 17},
        {aare::adc_sar_05_decode64to16, 29, 19},
        {aare::adc_sar_04_decode64to16, 15, 17},
    };

    for (const auto &c : cases) {
        uint64_t word = (1UL << c.bit0) | (1UL << c.bit1);
        std::vector<uint8_t> buffer(8);
        std::memcpy(buffer.data(), &word, sizeof(word));
        aare::NDView<const uint8_t, 2> bytes(buffer.data(), {1, 8});
        aare::NDArray<uint16_t, 2> out({1, 1});
        c.decode(bytes, out.view());
        CHECK(out(0, 0) == 3);
    }
}

TEST_CASE("Byte buffer decoding does not require an aligned buffer") {
    // Place the word three bytes into the buffer
    std::vector<uint8_t> buffer(3 + 8, 0);
    uint64_t word = (1UL << 29) | (1UL << 19);
    std::memcpy(buffer.data() + 3, &word, sizeof(word));

    aare::NDView<const uint8_t, 2> bytes(buffer.data() + 3, {1, 8});
    aare::NDArray<uint16_t, 2> out({1, 1});
    aare::adc_sar_05_decode64to16(bytes, out.view());
    CHECK(out(0, 0) == 3);
}

TEST_CASE("Byte buffer decoding rejects invalid shapes") {
    std::vector<uint8_t> buffer(24, 0);
    aare::NDArray<uint16_t, 2> out({1, 1});

    // Row length that is not a multiple of 8
    aare::NDView<const uint8_t, 2> odd_row(buffer.data(), {2, 12});
    REQUIRE_THROWS_AS(aare::adc_sar_05_decode64to16(odd_row, out.view()),
                      std::invalid_argument);

    // Output shape that does not match rows x (bytes / 8)
    aare::NDView<const uint8_t, 2> two_words(buffer.data(), {1, 16});
    REQUIRE_THROWS_AS(aare::adc_sar_05_decode64to16(two_words, out.view()),
                      std::invalid_argument);
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

TEST_CASE("Mask 32 bit unsigned integer to 24 bit") {
    // any number less than 2**24 (16777216) should be the same
    CHECK(aare::mask32to24bits(0) == 0);
    CHECK(aare::mask32to24bits(19) == 19);
    CHECK(aare::mask32to24bits(29875) == 29875);
    CHECK(aare::mask32to24bits(1092177) == 1092177);
    CHECK(aare::mask32to24bits(0xFFFF) == 0xFFFF);
    CHECK(aare::mask32to24bits(0xFFFFFFFF) == 0xFFFFFF);

    // Offset specifies that the should ignore 0-7 bits
    // at the start
    CHECK(aare::mask32to24bits(0xFFFF, BitOffset(4)) == 0xFFF);
    CHECK(aare::mask32to24bits(0xFF0000d9) == 0xd9);
    CHECK(aare::mask32to24bits(0xFF000d9F, BitOffset(4)) == 0xF000d9);
    CHECK(aare::mask32to24bits(16777217) == 1);
    CHECK(aare::mask32to24bits(15, BitOffset(7)) == 0);

    // Highest bit set to 1 should just be excluded
    // lowest 4 bits set to 1
    CHECK(aare::mask32to24bits(0x8000000f, BitOffset(7)) == 0);
}

TEST_CASE("Expand container with 24 bit data to 32") {
    {
        uint8_t buffer[] = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        };

        aare::NDView<uint8_t, 1> input(&buffer[0], {9});
        aare::NDArray<uint32_t, 1> out({3});
        aare::expand24to32bit(input, out.view());

        CHECK(out(0) == 0);
        CHECK(out(1) == 0);
        CHECK(out(2) == 0);
    }
    {
        uint8_t buffer[] = {
            0x0F, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF,
        };

        aare::NDView<uint8_t, 1> input(&buffer[0], {9});
        aare::NDArray<uint32_t, 1> out({3});
        aare::expand24to32bit(input, out.view());

        CHECK(out(0) == 0xF);
        CHECK(out(1) == 0xFF);
        CHECK(out(2) == 0xFFFFFF);
    }
    {
        uint8_t buffer[] = {
            0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00,
        };

        aare::NDView<uint8_t, 1> input(&buffer[0], {9});
        aare::NDArray<uint32_t, 1> out({3});
        aare::expand24to32bit(input, out.view());

        CHECK(out(0) == 0xFF0000);
        CHECK(out(1) == 0xFFFF);
        CHECK(out(2) == 0xFF00);

        REQUIRE_THROWS(aare::expand24to32bit(input, out.view(), BitOffset(4)));
    }
    {
        // For use with offset we need an extra byte
        uint8_t buffer[] = {0x00, 0x00, 0xFF, 0xFF, 0xFF,
                            0x00, 0x00, 0xFF, 0x00, 0x00};

        aare::NDView<uint8_t, 1> input(&buffer[0], {10});
        aare::NDArray<uint32_t, 1> out({3}); // still output.size == 3
        aare::expand24to32bit(input, out.view(), BitOffset(4));

        CHECK(out(0) == 0xFFF000);
        CHECK(out(1) == 0xFFF);
        CHECK(out(2) == 0xFF0);
    }
}

TEST_CASE("Expand 24 bit values to 32 bit values from a const buffer") {
    const uint8_t buffer[] = {
        0x0F, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF,
    };

    aare::NDView<const uint8_t, 1> input(buffer, {9});
    aare::NDArray<uint32_t, 1> out({3});
    aare::expand24to32bit(input, out.view());

    CHECK(out(0) == 0xF);
    CHECK(out(1) == 0xFF);
    CHECK(out(2) == 0xFFFFFF);
}

TEST_CASE("Expand 4 bit values packed into 8 bit to 8 bit values") {
    {
        uint8_t buffer[] = {
            0x00, 0xF0, 0xFF, 0x00, 0xF0, 0xFF,
        };

        aare::NDView<uint8_t, 1> input(&buffer[0], {6});
        aare::NDArray<uint8_t, 1> out({12});
        aare::expand4to8bit(input, out.view());

        uint8_t expected_output[] = {
            0x0, 0x0, 0x0, 0xF, 0xF, 0xF,
            0x0, 0x0, 0x0, 0xF, 0xF, 0xF}; // assuming little endian

        for (size_t i = 0; i < 12; ++i) {
            CHECK(out(i) == expected_output[i]);
        }
    }
}

TEST_CASE("Expand 4 bit values packed into 8 bit to 8 bit values from a const "
          "buffer") {
    {
        const uint8_t buffer[] = {
            0x00, 0xF0, 0xFF, 0x00, 0xF0, 0xFF,
        };

        aare::NDView<const uint8_t, 1> input(&buffer[0], {6});
        aare::NDArray<uint8_t, 1> out({12});
        aare::expand4to8bit(input, out.view());

        uint8_t expected_output[] = {
            0x0, 0x0, 0x0, 0xF, 0xF, 0xF,
            0x0, 0x0, 0x0, 0xF, 0xF, 0xF}; // assuming little endian

        for (size_t i = 0; i < 12; ++i) {
            CHECK(out(i) == expected_output[i]);
        }
    }
}
