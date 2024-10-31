#include "aare/PixelMap.hpp"

#include <array>

namespace aare {
NDArray<size_t, 2> GenerateMoench03PixelMap() {
    std::array<int, 32> const adc_nr = {300, 325, 350, 375, 300, 325, 350, 375, 200, 225, 250,
                                        275, 200, 225, 250, 275, 100, 125, 150, 175, 100, 125,
                                        150, 175, 0,   25,  50,  75,  0,   25,  50,  75};
    int const sc_width = 25;
    int const nadc = 32;
    int const pixels_per_sc = 5000;
    NDArray<size_t, 2> order_map({400, 400});

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
} // namespace aare