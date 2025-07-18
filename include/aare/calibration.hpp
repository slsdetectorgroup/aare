#pragma once

#include "aare/defs.hpp"
#include "aare/utils/task.hpp"
#include <cstdint>
#include <future>

namespace aare {

// Really try to convince the compile to inline this function
// TODO! Clang?
#if (defined(_MSC_VER) || defined(__INTEL_COMPILER))
#define STRONG_INLINE __forceinline
#else
#define STRONG_INLINE inline
#endif

#if defined(__GNUC__)
#define ALWAYS_INLINE __attribute__((always_inline)) inline
#else
#define ALWAYS_INLINE STRONG_INLINE
#endif

/**
 * @brief Get the gain from the raw ADC value. In Jungfrau the gain is
 * encoded in the left most 2 bits of the raw value.
 * 00 -> gain 0
 * 01 -> gain 1
 * 11 -> gain 2
 * @param raw the raw ADC value
 * @return the gain as an integer
 */
ALWAYS_INLINE int get_gain(uint16_t raw) {
    switch (raw >> 14) {
    case 0:
        return 0;
    case 1:
        return 1;
    case 3:
        return 2;
    default:
        return 0;
    }
}

ALWAYS_INLINE uint16_t get_value(uint16_t raw) { return raw & ADC_MASK; }

ALWAYS_INLINE std::pair<uint16_t, int16_t> get_value_and_gain(uint16_t raw) {
    static_assert(
        sizeof(std::pair<uint16_t, int16_t>) ==
            sizeof(uint16_t) + sizeof(int16_t),
        "Size of pair<uint16_t, int16_t> does not match expected size");
    return {get_value(raw), get_gain(raw)};
}

template <class T>
void apply_calibration_impl(NDView<T, 3> res, NDView<uint16_t, 3> raw_data,
                       NDView<T, 3> ped, NDView<T, 3> cal, int start,
                       int stop) {

    for (int frame_nr = start; frame_nr != stop; ++frame_nr) {
        for (int row = 0; row != raw_data.shape(1); ++row) {
            for (int col = 0; col != raw_data.shape(2); ++col) {
                auto [value, gain] = get_value_and_gain(raw_data(frame_nr, row, col));
                res(frame_nr, row, col) =
                    (value - ped(gain, row, col)) / cal(gain, row, col); //TODO! use multiplication
            }
        }
    }
}

template <class T>
void apply_calibration(NDView<T, 3> res, NDView<uint16_t, 3> raw_data,
                       NDView<T, 3> ped, NDView<T, 3> cal,
                       ssize_t n_threads = 4) {
    std::vector<std::future<void>> futures;
    futures.reserve(n_threads);
    auto limits = split_task(0, raw_data.shape(0), n_threads);
    for (const auto &lim : limits)
        futures.push_back(std::async(&apply_calibration_impl<T>, res, raw_data, ped, cal,
                                     lim.first, lim.second));
    for (auto &f : futures)
        f.get();
}

} // namespace aare