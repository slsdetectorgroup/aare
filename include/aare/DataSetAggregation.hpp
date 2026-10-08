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
    File meta_file(path);

    auto frame = meta_file.read_frame();

    NDArray<FRAME_TYPE, 3> mean_per_sample(
        {static_cast<ssize_t>(meta_file.rows()),
         static_cast<ssize_t>(meta_file.cols()),
         static_cast<ssize_t>(num_samples)});

    auto mean_over_samples = [&](size_t start_sample, size_t end_sample) {
        File file(path);

        file.seek(start_sample * num_frames_per_sample);
        NDArray<FRAME_TYPE, 3> frames(
            {static_cast<ssize_t>(num_frames_per_sample),
             static_cast<ssize_t>(file.rows()),
             static_cast<ssize_t>(file.cols())});

        size_t rows = file.rows();
        size_t cols = file.cols();

        std::vector<double> sum(rows *
                                cols); // need to be double to avoid overflow
                                       // for uint16_t and uint32_t
        const double inv_n = 1.0 / static_cast<double>(num_frames_per_sample);

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

    for (int i = 0; i < num_threads - 1; ++i) {
        int end_sample =
            std::min((i + 1) * static_cast<size_t>(chunk_size), num_samples);

        futures[i] = std::async(std::launch::async, mean_over_samples,
                                i * chunk_size, end_sample);
    }

    int end_sample = num_samples;
    futures[num_threads - 1] =
        std::async(std::launch::async, mean_over_samples,
                   (num_threads - 1) * chunk_size, end_sample);

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

/*
 * @brief Compute the mean current source data from a Jungfrau detector file
 * @param path The path to the Jungfrau detector file.
 * @param mean_over_num_frames The number of frames to mean over for each
 * column.
 * @param num_samples The number of samples in the dataset e.g. exposure times
 * @param num_threads The number of threads to use for parallel computation.
 * @param file_offset The offset in the file to start reading from.
 * @return An NDArray containing the mean current source data for each sample.
 */
NDArray<uint16_t, 3> mean_current_source(const std::filesystem::path &path,
                                         const size_t mean_over_num_frames,
                                         const size_t num_samples,
                                         const int num_threads = 16,
                                         size_t file_offset = 0) {

    File meta_file(path);

    size_t total_num_frames = meta_file.total_frames();

    const size_t num_rows = meta_file.rows();
    const size_t num_cols = meta_file.cols();
    const size_t half_rows = num_rows / 2;

    constexpr size_t columns_per_subcolumn = 64;

    constexpr size_t num_subcolumns = 16;

    if (total_num_frames !=
        mean_over_num_frames * num_samples * columns_per_subcolumn +
            file_offset) {
        throw std::runtime_error(fmt::format(
            "Frames mismatch: Expected {} frames got {}",
            mean_over_num_frames * num_samples * columns_per_subcolumn +
                file_offset,
            total_num_frames));
    }

    const double inv_mean_over_num_frames =
        1.0 / static_cast<double>(mean_over_num_frames);

    NDArray<uint16_t, 3> cs_data({static_cast<ssize_t>(num_samples),
                                  static_cast<ssize_t>(num_rows),
                                  static_cast<ssize_t>(num_cols)},
                                 0.0);

    auto mean_over_samples = [&](size_t start_sample, size_t end_sample) {
        File file(path);

        file.seek(start_sample * mean_over_num_frames * columns_per_subcolumn +
                  file_offset);

        NDArray<uint16_t, 2> frame(
            {static_cast<ssize_t>(num_rows), static_cast<ssize_t>(num_cols)});

        std::vector<double> sum(num_rows * num_cols,
                                0.0); // need to be double to avoid
                                      // overflow for uint16_t

        for (size_t i = start_sample; i < end_sample; ++i) {

            std::fill(sum.begin(), sum.end(), 0.0);

            for (size_t acq_idx = 0; acq_idx < mean_over_num_frames;
                 ++acq_idx) {
                for (size_t col_idx = 0; col_idx < columns_per_subcolumn;
                     ++col_idx) {
                    file.read_into(reinterpret_cast<std::byte *>(frame.data()));

                    const uint16_t *frame_data_ptr = frame.data();

                    for (size_t r = 0; r < half_rows; ++r) {
                        const size_t row_offset = r * num_cols;
                        for (size_t subcol_idx = 0; subcol_idx < num_subcolumns;
                             ++subcol_idx) {

                            const size_t absolute_col_idx =
                                subcol_idx * columns_per_subcolumn +
                                col_idx; // TODO: maybe precompute and use a
                                         // lookup table for this
                            sum[row_offset + absolute_col_idx] +=
                                frame_data_ptr[row_offset + absolute_col_idx];
                        }
                    }

                    for (size_t r = half_rows; r < num_rows; ++r) {
                        const size_t row_offset = r * num_cols;
                        for (size_t subcol_idx = 0; subcol_idx < num_subcolumns;
                             ++subcol_idx) {

                            const size_t absolute_col_idx =
                                (subcol_idx + 1) * columns_per_subcolumn -
                                col_idx;
                            sum[row_offset + absolute_col_idx] +=
                                frame_data_ptr[row_offset + absolute_col_idx];
                        }
                    }
                }
            }

            uint16_t *cs_data_ptr = cs_data.data() + i * num_rows * num_cols;
            for (size_t idx = 0; idx < num_rows * num_cols; ++idx) {
                cs_data_ptr[idx] =
                    static_cast<uint16_t>(sum[idx] * inv_mean_over_num_frames);
            }
        }
    };

    std::vector<std::future<void>> futures(num_threads);

    const size_t chunk_size = num_samples / num_threads;

    for (int i = 0; i < num_threads - 1; ++i) {
        size_t end_sample = std::min((i + 1) * chunk_size, num_samples);

        futures[i] = std::async(std::launch::async, mean_over_samples,
                                i * chunk_size, end_sample);
    }

    int end_sample = num_samples;
    futures[num_threads - 1] =
        std::async(std::launch::async, mean_over_samples,
                   (num_threads - 1) * chunk_size, end_sample);

    for (auto &f : futures) {
        f.get();
    }

    return cs_data;
}

} // namespace aare