// SPDX-License-Identifier: MPL-2.0
#include "aare/PixelMap.hpp"

#include <array>

namespace aare {
NDArray<ssize_t, 2> GenerateMoench03PixelMap() {
    std::array<int, 32> const adc_nr = {300, 325, 350, 375, 300, 325, 350, 375,
                                        200, 225, 250, 275, 200, 225, 250, 275,
                                        100, 125, 150, 175, 100, 125, 150, 175,
                                        0,   25,  50,  75,  0,   25,  50,  75};
    int const sc_width = 25;
    int const nadc = 32;
    int const pixels_per_sc = 5000;
    NDArray<ssize_t, 2> order_map({400, 400});

    int pixel = 0;
    for (int i = 0; i != pixels_per_sc; ++i) {
        for (int i_adc = 0; i_adc != nadc; ++i_adc) {
            int const col = adc_nr[i_adc] + (i % sc_width);
            int row = 0;
            if ((i_adc / 4) % 2 == 0)
                row = 199 - (i / sc_width);
            else
                row = 200 + (i / sc_width);

            order_map(row, col) = pixel;
            pixel++;
        }
    }
    return order_map;
}

NDArray<ssize_t, 2> GenerateMoench04AnalogPixelMap() {
    std::array<int, 32> const adc_nr = {
        9,  8,  11, 10, 13, 12, 15, 14, 1,  0,  3,  2,  5,  4,  7,  6,
        23, 22, 21, 20, 19, 18, 17, 16, 31, 30, 29, 28, 27, 26, 25, 24};
    int const sc_width = 25;
    int const nadc = 32;
    int const pixels_per_sc = 5000;
    NDArray<ssize_t, 2> order_map({400, 400});

    int pixel = 0;
    for (int i = 0; i != pixels_per_sc; ++i) {
        for (int i_adc = 0; i_adc != nadc; ++i_adc) {
            int const col = (adc_nr[i_adc] % 16) * 25 + (i % sc_width);
            int row = 0;
            if (i_adc < 16)
                row = 199 - (i / sc_width);
            else
                row = 200 + (i / sc_width);

            order_map(row, col) = pixel;
            pixel++;
        }
    }
    return order_map;
}

NDArray<ssize_t, 2> GenerateMoench05PixelMap() {
    std::array<int, 3> adc_numbers = {5, 9, 1};
    NDArray<ssize_t, 2> order_map({160, 150});
    int n_pixel = 0;
    for (int row = 0; row < 160; row++) {
        for (int i_col = 0; i_col < 50; i_col++) {
            n_pixel = row * 50 + i_col;
            for (int i_sc = 0; i_sc < 3; i_sc++) {
                int col = 50 * i_sc + i_col;
                int adc_nr = adc_numbers[i_sc];
                int i_analog = n_pixel * 12 + adc_nr;

                // analog_frame[row * 150 + col] = analog_data[i_analog] &
                // 0x3FFF;
                order_map(row, col) = i_analog;
            }
        }
    }
    return order_map;
}

NDArray<ssize_t, 2> GenerateMoench05PixelMap1g() {
    std::array<int, 3> adc_numbers = {1, 2, 0};
    NDArray<ssize_t, 2> order_map({160, 150});
    int n_pixel = 0;
    for (int row = 0; row < 160; row++) {
        for (int i_col = 0; i_col < 50; i_col++) {
            n_pixel = row * 50 + i_col;
            for (int i_sc = 0; i_sc < 3; i_sc++) {
                int col = 50 * i_sc + i_col;
                int adc_nr = adc_numbers[i_sc];
                int i_analog = n_pixel * 3 + adc_nr;

                // analog_frame[row * 150 + col] = analog_data[i_analog] &
                // 0x3FFF;
                order_map(row, col) = i_analog;
            }
        }
    }
    return order_map;
}

NDArray<ssize_t, 2> GenerateMoench05PixelMapOld() {
    std::array<int, 3> adc_numbers = {9, 13, 1};
    NDArray<ssize_t, 2> order_map({160, 150});
    int n_pixel = 0;
    for (int row = 0; row < 160; row++) {
        for (int i_col = 0; i_col < 50; i_col++) {
            n_pixel = row * 50 + i_col;
            for (int i_sc = 0; i_sc < 3; i_sc++) {
                int col = 50 * i_sc + i_col;
                int adc_nr = adc_numbers[i_sc];
                int i_analog = n_pixel * 32 + adc_nr;

                // analog_frame[row * 150 + col] = analog_data[i_analog] &
                // 0x3FFF;
                order_map(row, col) = i_analog;
            }
        }
    }
    return order_map;
}

NDArray<ssize_t, 2> GenerateEigerFlipRowsPixelMap() {
    NDArray<ssize_t, 2> order_map({256, 512});
    for (int row = 0; row < 256; row++) {
        for (int col = 0; col < 512; col++) {
            order_map(row, col) = 255 * 512 - row * 512 + col;
        }
    }
    return order_map;
}

// transceiver pixel map for Matterhorn02
NDArray<ssize_t, 2> GenerateMH02SingleCounterPixelMap() {
    // This is the pixel map for a single counter Matterhorn02, i.e. 48x48
    // pixels. Data is read from two transceivers in blocks of 4 pixels.
    NDArray<ssize_t, 2> order_map({48, 48});
    size_t offset = 0;
    size_t nSamples = 4;
    for (int row = 0; row < 48; row++) {
        for (int col = 0; col < 24; col++) {
            for (int iTrans = 0; iTrans < 2; iTrans++) {
                order_map(row, iTrans * 24 + col) = offset + nSamples * iTrans;
            }
            offset += 1;
            if ((col + 1) % nSamples == 0) {
                offset += nSamples;
            }
        }
    }
    return order_map;
}

NDArray<ssize_t, 3> GenerateMH02FourCounterPixelMap() {
    auto single_counter_map = GenerateMH02SingleCounterPixelMap();
    NDArray<ssize_t, 3> order_map({4, 48, 48});
    for (int counter = 0; counter < 4; counter++) {
        for (int row = 0; row < 48; row++) {
            for (int col = 0; col < 48; col++) {
                order_map(counter, row, col) =
                    single_counter_map(row, col) + counter * 48 * 48;
            }
        }
    }
    return order_map;
}

NDArray<ssize_t, 2> GenerateMatterhorn10PixelMap(const size_t dynamic_range,
                                                 const size_t n_counters) {
    constexpr size_t n_cols = 256;
    constexpr size_t n_rows = 256;
    NDArray<ssize_t, 2> pixel_map(
        {static_cast<ssize_t>(n_rows * n_counters), n_cols});

    size_t num_consecutive_pixels{};
    switch (dynamic_range) {
    case 16:
        num_consecutive_pixels = 4;
        break;
    case 8:
        num_consecutive_pixels = 8;
        break;
    case 4:
        num_consecutive_pixels = 16;
        break;
    default:
        throw std::runtime_error("Unsupported dynamic range for Matterhorn02");
    }

    for (size_t row = 0; row < n_rows; ++row) {
        for (size_t counter = 0; counter < n_counters; ++counter) {
            size_t col = 0;
            for (size_t offset = 0; offset < 64;
                 offset += num_consecutive_pixels) {
                for (size_t pkg = offset; pkg < 256; pkg += 64) {
                    for (size_t pixel = 0; pixel < num_consecutive_pixels;
                         ++pixel) {
                        pixel_map(row + counter * n_rows, col) =
                            pkg + pixel +
                            counter * n_rows * n_cols * n_counters;
                        ++col;
                    }
                }
            }
        }
    }
    return pixel_map;
}

} // namespace aare