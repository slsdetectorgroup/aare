#include "aare/calibration.hpp"

namespace aare {

NDArray<int, 2> count_switching_pixels(NDView<uint16_t, 3> raw_data) {
    NDArray<int, 2> switched(
        std::array<ssize_t, 2>{raw_data.shape(1), raw_data.shape(2)}, 0);
    for (int frame_nr = 0; frame_nr != raw_data.shape(0); ++frame_nr) {
        for (int row = 0; row != raw_data.shape(1); ++row) {
            for (int col = 0; col != raw_data.shape(2); ++col) {
                auto [value, gain] =
                    get_value_and_gain(raw_data(frame_nr, row, col));
                if (gain != 0) {
                    switched(row, col) += 1;
                }
            }
        }
    }
    return switched;
}

NDArray<int, 2> count_switching_pixels(NDView<uint16_t, 3> raw_data,
                                       ssize_t n_threads) {
    NDArray<int, 2> switched(
        std::array<ssize_t, 2>{raw_data.shape(1), raw_data.shape(2)}, 0);
    std::vector<std::future<NDArray<int, 2>>> futures;
    futures.reserve(n_threads);

    auto subviews = make_subviews(raw_data, n_threads);

    for (auto view : subviews) {
        futures.push_back(
            std::async(static_cast<NDArray<int, 2> (*)(NDView<uint16_t, 3>)>(
                           &count_switching_pixels),
                       view));
    }

    for (auto &f : futures) {
        switched += f.get();
    }
    return switched;
}

} // namespace aare