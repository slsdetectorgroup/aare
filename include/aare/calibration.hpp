#pragma once

#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
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
                auto [value, gain] =
                    get_value_and_gain(raw_data(frame_nr, row, col));

                // Using multiplication does not seem to speed up the code here
                // ADU/keV is the standard unit for the calibration which
                // means rewriting the formula is not worth it.
                res(frame_nr, row, col) =
                    (value - ped(gain, row, col)) / cal(gain, row, col);
            }
        }
    }
}

template <class T>
void apply_calibration_impl(NDView<T, 3> res, NDView<uint16_t, 3> raw_data,
                            NDView<T, 2> ped, NDView<T, 2> cal, int start,
                            int stop) {

    for (int frame_nr = start; frame_nr != stop; ++frame_nr) {
        for (int row = 0; row != raw_data.shape(1); ++row) {
            for (int col = 0; col != raw_data.shape(2); ++col) {
                auto [value, gain] =
                    get_value_and_gain(raw_data(frame_nr, row, col));

                // Using multiplication does not seem to speed up the code here
                // ADU/keV is the standard unit for the calibration which
                // means rewriting the formula is not worth it.

                // ignore anything that is not gain 0
                // TODO! deal with fixed gain?
                if (gain == 0)
                    res(frame_nr, row, col) =
                        (value - ped(row, col)) / cal(row, col);
            }
        }
    }
}

template <class T, ssize_t Ndim=3>
void apply_calibration(NDView<T, 3> res, NDView<uint16_t, 3> raw_data,
                       NDView<T, Ndim> ped, NDView<T, Ndim> cal,
                       ssize_t n_threads = 4) {
    std::vector<std::future<void>> futures;
    futures.reserve(n_threads);
    auto limits = split_task(0, raw_data.shape(0), n_threads);
    for (const auto &lim : limits)
        futures.push_back(std::async(
            static_cast<void (*)(NDView<T, 3>, NDView<uint16_t, 3>,
                                 NDView<T, Ndim>, NDView<T, Ndim>, int, int)>(
                apply_calibration_impl),
            res, raw_data, ped, cal, lim.first, lim.second));
    for (auto &f : futures)
        f.get();
}

std::pair<NDArray<size_t, 3>, NDArray<size_t, 3>>
sum_and_count_per_gain(NDView<uint16_t, 3> raw_data);

std::pair<NDArray<size_t, 2>, NDArray<size_t, 2>>
sum_and_count_g0(NDView<uint16_t, 3> raw_data);


template <typename T>
NDArray<T, 2> calculate_pedestal_g0(NDView<uint16_t, 3> raw_data,
                                 ssize_t n_threads) {
    std::vector<std::future<std::pair<NDArray<size_t, 2>, NDArray<size_t, 2>>>>
        futures;
    futures.reserve(n_threads);
    auto limits = split_task(0, raw_data.shape(0), n_threads);  
    // make subviews for each thread
    std::vector<NDView<uint16_t, 3>> subviews;
    for (const auto &lim : limits) {
        subviews.emplace_back(
            raw_data.data() + lim.first * raw_data.strides()[0],
            std::array<ssize_t, 3>{lim.second - lim.first, raw_data.shape(1),
                                   raw_data.shape(2)});
    }
    for (auto view : subviews) {
        futures.push_back(std::async(
            static_cast<std::pair<NDArray<size_t, 2>, NDArray<size_t, 2>> (*)(
                NDView<uint16_t, 3>)>(&sum_and_count_g0),
            view));
    }
    NDArray<size_t, 2> accumulator(
        std::array<ssize_t, 2>{raw_data.shape(1), raw_data.shape(2)}, 0);
    NDArray<size_t, 2> count(
        std::array<ssize_t, 2>{raw_data.shape(1), raw_data.shape(2)}, 0);
    for (auto &f : futures) {
        auto [acc, cnt] = f.get();
        accumulator += acc;
        count += cnt;
    }

    return safe_divide<T>(accumulator, count);

}


template <typename T>
NDArray<T, 3> calculate_pedestal(NDView<uint16_t, 3> raw_data,
                                 ssize_t n_threads) {
    std::vector<std::future<std::pair<NDArray<size_t, 3>, NDArray<size_t, 3>>>>
        futures;
    futures.reserve(n_threads);
    auto limits = split_task(0, raw_data.shape(0), n_threads);

    // make subviews for each thread
    std::vector<NDView<uint16_t, 3>> subviews;
    for (const auto &lim : limits) {
        subviews.emplace_back(
            raw_data.data() + lim.first * raw_data.strides()[0],
            std::array<ssize_t, 3>{lim.second - lim.first, raw_data.shape(1),
                                   raw_data.shape(2)});
    }
    for (auto view : subviews) {
        futures.push_back(std::async(
            static_cast<std::pair<NDArray<size_t, 3>, NDArray<size_t, 3>> (*)(
                NDView<uint16_t, 3>)>(&sum_and_count_per_gain),
            view));
    }

    NDArray<size_t, 3> accumulator(
        std::array<ssize_t, 3>{3, raw_data.shape(1), raw_data.shape(2)}, 0);
    NDArray<size_t, 3> count(
        std::array<ssize_t, 3>{3, raw_data.shape(1), raw_data.shape(2)}, 0);
    for (auto &f : futures) {
        auto [acc, cnt] = f.get();
        accumulator += acc;
        count += cnt;
    }

    return safe_divide<T>(accumulator, count);
}

/**
 * @brief Count the number of switching pixels in the raw data.
 * This function counts the number of pixels that switch between G1 and G2 gain.
 * It returns an NDArray with the number of switching pixels per pixel.
 * @param raw_data The NDView containing the raw data
 * @return An NDArray with the number of switching pixels per pixel
 */
NDArray<int, 2> count_switching_pixels(NDView<uint16_t, 3> raw_data);

/**
 * @brief Count the number of switching pixels in the raw data.
 * This function counts the number of pixels that switch between G1 and G2 gain.
 * It returns an NDArray with the number of switching pixels per pixel.
 * @param raw_data The NDView containing the raw data
 * @param n_threads The number of threads to use for parallel processing
 * @return An NDArray with the number of switching pixels per pixel
 */
NDArray<int, 2> count_switching_pixels(NDView<uint16_t, 3> raw_data,
                                       ssize_t n_threads);

} // namespace aare