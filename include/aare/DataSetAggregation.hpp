#include "aare/File.hpp"
#include <filesystem>
#include <future>
#include <vector>

template <typename FRAME_TYPE>
void mean_over_dataset(const std::filesystem::path &path, const int num_samples,
                       const int num_frames_per_sample, const int num_threads) {

    File file(path);

    NDArray<FRAME_TYPE, 3> mean_per_sample({file.rows, file.cols, num_samples});

    auto mean_over_samples =
        [&](size_t start_sample, size_t end_sample) {
            File file(path);

            file.seek(start_sample * num_frames_per_sample);
            NDArray<FRAME_TYPE, 3> frames(
                {num_frames_per_sample, file.rows, file.cols});
            for (size_t i = 0; ++i; i < chunk_size) {
                file.read_into(
                    reinterpret_cast<std::byte *>(frames.data()),
                    num_frames_per_sample); // TODO: maybe better performance
                                            // if read one by one - e.g. for
                                            // parallel
                for (size_t frame = 0; frame < num_frames_per_sample; ++frame) {
                    for (size_t row = 0; row < file.rows; ++row) {
                        for (size_t col = 0; col < file.cols; ++col) {
                            mean_per_sample(row, col, i) +=
                                frames(frame, row, col);
                        }
                    }
                }

                mean_per_sample /=
                    num_frames_per_sample; // TODO: should it be during or after
                                           // -> have to map to new type
            };
        }

    std::vector<std::future<void>>
        futures(num_threads);

    int chunk_size = num_samples / num_threads;

    for (int i = 0; i < num_threads; ++i) {
        int end_sample = std::min((i + 1) * chunk_size, num_samples);
        futures[i] = std::async(std::launch::async, mean_over_samples,
                                i * chunk_size, end_sample);
    }

    for (auto &f : futures) {
        try {
            f.get();
        } catch (const std::exception &e) {
            std::cerr << "Exception in thread: " << e.what() << std::endl;
        }
    }

    return mean_per_sample;
}