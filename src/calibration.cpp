#include "aare/calibration.hpp"

namespace aare {

std::pair<NDArray<size_t, 3>, NDArray<size_t,3>> sum_and_count_per_gain(NDView<uint16_t,3> raw_data){
    NDArray<size_t, 3> accumulator(std::array<ssize_t, 3>{3, raw_data.shape(1), raw_data.shape(2)}, 0);
    NDArray<size_t, 3> count(std::array<ssize_t, 3>{3, raw_data.shape(1), raw_data.shape(2)}, 0);
    for (int frame_nr = 0; frame_nr != raw_data.shape(0); ++frame_nr) {
        for (int row = 0; row != raw_data.shape(1); ++row) {
            for (int col = 0; col != raw_data.shape(2); ++col) {
                auto [value, gain] = get_value_and_gain(raw_data(frame_nr, row, col));
                accumulator(gain, row, col) += value;
                count(gain, row, col) += 1;
            }
        }
    }

    return {std::move(accumulator), std::move(count)};
}

std::pair<NDArray<size_t, 2>, NDArray<size_t,2>> sum_and_count_g0(NDView<uint16_t, 3> raw_data){
    NDArray<size_t, 2> accumulator(std::array<ssize_t, 2>{raw_data.shape(1), raw_data.shape(2)}, 0);
    NDArray<size_t, 2> count(std::array<ssize_t, 2>{raw_data.shape(1), raw_data.shape(2)}, 0);
    for (int frame_nr = 0; frame_nr != raw_data.shape(0); ++frame_nr) {
        for (int row = 0; row != raw_data.shape(1); ++row) {
            for (int col = 0; col != raw_data.shape(2); ++col) {
                auto [value, gain] = get_value_and_gain(raw_data(frame_nr, row, col));
                if (gain != 0)
                    continue; // we only care about gain 0
                accumulator(row, col) += value;
                count(row, col) += 1;
            }
        }
    }

    return {std::move(accumulator), std::move(count)};
}

NDArray<int, 2> count_switching_pixels(NDView<uint16_t, 3> raw_data) {
    NDArray<int, 2> switched(std::array<ssize_t, 2>{raw_data.shape(1), raw_data.shape(2)},0);
    for (int frame_nr = 0; frame_nr != raw_data.shape(0); ++frame_nr) {
        for (int row = 0; row != raw_data.shape(1); ++row) {
            for (int col = 0; col != raw_data.shape(2); ++col) {
                auto [value, gain] = get_value_and_gain(raw_data(frame_nr, row, col));
                if (gain != 0) {
                    switched(row, col) += 1;
                }
            }
        }
    }
    return switched;
}


NDArray<int, 2> count_switching_pixels(NDView<uint16_t, 3> raw_data, ssize_t n_threads){
    NDArray<int, 2> switched(std::array<ssize_t, 2>{raw_data.shape(1), raw_data.shape(2)},0);
    std::vector<std::future<NDArray<int, 2>>> futures;
    futures.reserve(n_threads);
    auto limits = split_task(0, raw_data.shape(0), n_threads);

    // make subviews for each thread
    std::vector<NDView<uint16_t, 3>> subviews;
    for (const auto &lim : limits) {
        subviews.emplace_back(raw_data.data() + lim.first * raw_data.strides()[0],
                              std::array<ssize_t, 3>{lim.second-lim.first, raw_data.shape(1), raw_data.shape(2)});
    }

    for (auto view : subviews) {
        futures.push_back(std::async(static_cast<NDArray<int, 2>(*)(NDView<uint16_t, 3>)>(&count_switching_pixels), view));
    }
    
    for (auto &f : futures) {
        switched += f.get();
    }
    return switched;
}

}// namespace aare