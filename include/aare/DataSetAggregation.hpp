#pragma once
#include "aare/File.hpp"
#include <algorithm>
#include <filesystem>
#include <future>
#include <vector>

namespace aare {

/**
 * @brief Compute the mean over several frames per sample stored in a file
 * @tparam FRAME_TYPE The type of the frames in the dataset.
 * @param path The path to the file containing the dataset.
 * @param num_samples The number of samples in the dataset.
 * @param num_frames_per_sample The number of frames per sample.
 * @param num_threads The number of threads to use for parallel computation.
 * @return An NDArray containing the mean frame for each sample.
 */
template <typename FRAME_TYPE>
NDArray<FRAME_TYPE, 3> mean_over_dataset(const std::filesystem::path &path,
                                         const size_t num_samples,
                                         const size_t num_frames_per_sample,
                                         const int num_threads = 16) {

    size_t rows = 0;
    size_t cols = 0;
    size_t total_num_frames = 0;

    {
        File meta_file(path);
        rows = meta_file.rows();
        cols = meta_file.cols();
        total_num_frames = meta_file.total_frames();
    }

    NDArray<FRAME_TYPE, 3> mean_per_sample({static_cast<ssize_t>(rows),
                                            static_cast<ssize_t>(cols),
                                            static_cast<ssize_t>(num_samples)});

    if (num_threads < 1)
        throw std::invalid_argument("num_threads must be at least 1");

    if (total_num_frames != num_samples * num_frames_per_sample) {
        throw std::runtime_error(
            fmt::format("Frames mismatch: Expected {} frames got {}",
                        num_samples * num_frames_per_sample, total_num_frames));
    }

    const double inv_n = 1.0 / static_cast<double>(num_frames_per_sample);

    auto mean_over_samples = [&](size_t start_sample, size_t end_sample) {
        File file(path);

        file.seek(start_sample * num_frames_per_sample);
        NDArray<FRAME_TYPE, 3> frames(
            {static_cast<ssize_t>(num_frames_per_sample),
             static_cast<ssize_t>(rows), static_cast<ssize_t>(cols)});

        std::vector<double> sum(rows *
                                cols); // need to be double to avoid overflow
                                       // for uint16_t and uint32_t

        // TODO: maybe tile
        for (size_t i = start_sample; i < end_sample; ++i) {
            file.read_into(reinterpret_cast<std::byte *>(frames.data()),
                           num_frames_per_sample); // TODO: maybe better
                                                   // performance if read one by
                                                   // one - e.g. for parallel

            std::fill(sum.begin(), sum.end(), 0.0);

            const FRAME_TYPE *src =
                frames.data(); // using the pointer seems to be much faster than
                               // using the NDArray

            for (size_t f = 0; f < num_frames_per_sample; ++f) {
                const size_t f_off = f * rows * cols;
                for (size_t r = 0; r < rows; ++r) {
                    const size_t off = f_off + r * cols;
                    for (size_t c = 0; c < cols; ++c) {
                        sum[r * cols + c] += static_cast<double>(src[off + c]);
                    }
                }
            }

            for (size_t r = 0; r < rows; ++r) {
                for (size_t c = 0; c < cols; ++c) {
                    mean_per_sample(r, c, i) =
                        static_cast<FRAME_TYPE>(sum[r * cols + c] * inv_n);
                }
            }
        };
    };

    std::vector<std::future<void>> futures(num_threads);

    int chunk_size = num_samples / num_threads;

    for (int i = 0; i < num_threads; ++i) {
        size_t end_sample =
            (i == num_threads - 1) ? num_samples : (i + 1) * chunk_size;

        futures[i] = std::async(std::launch::async, mean_over_samples,
                                i * chunk_size, end_sample);
    }

    for (auto &f : futures) {
        try {
            f.get();
        } catch (const std::exception &e) {
            throw std::runtime_error(std::string("Exception in thread: ") +
                                     e.what());
        }
    }

    return mean_per_sample;
}

/**
 * @brief Compute the mean current source data from a Jungfrau detector file
 * @param path The path to the Jungfrau detector file.
 * @param mean_over_num_frames The number of frames to mean over for each
 * column.
 * @param num_samples The number of samples in the dataset e.g. exposure times
 * @param num_threads The number of threads to use for parallel computation.
 * @param file_offset The offset in the file to start reading from.
 * @return An NDArray containing the mean current source data for each sample.
 (rows, cols, num_samples)

 */
NDArray<uint16_t, 3> mean_current_source(const std::filesystem::path &path,
                                         const size_t mean_over_num_frames,
                                         const size_t num_samples,
                                         const int num_threads = 16,
                                         size_t file_offset = 0);

} // namespace aare
