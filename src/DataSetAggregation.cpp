// SPDX-License-Identifier: MPL-2.0
#include "aare/DataSetAggregation.hpp"
#include "aare/JungfrauDataFile.hpp"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fmt/format.h>
#include <future>
#include <memory>
#include <stdexcept>
#include <vector>

namespace aare {

// TODO: maybe O_DIRECT rea dis better to avoid the page cache

NDArray<double, 3> mean_current_source(const std::filesystem::path &path,
                                       const size_t num_samples,
                                       const size_t mean_over_num_frames,
                                       const int num_threads,
                                       size_t file_offset) {

    constexpr size_t columns_per_subcolumn = 64;
    constexpr size_t frames_per_chunk = 16; // ~16 MiB per read for 512x1024

    size_t num_rows = 0;
    size_t num_cols = 0;
    size_t total_num_frames = 0;
    {
        JungfrauDataFile meta_file(path);
        num_rows = meta_file.rows();
        num_cols = meta_file.cols();
        total_num_frames = meta_file.total_frames();
    }

    const size_t half_rows = num_rows / 2;
    const size_t num_subcolumns = num_cols / columns_per_subcolumn;
    const size_t frames_per_sample =
        mean_over_num_frames * columns_per_subcolumn;

    if (total_num_frames != frames_per_sample * num_samples + file_offset) {
        throw std::runtime_error(fmt::format(
            "Frames mismatch: Expected {} frames got {}",
            frames_per_sample * num_samples + file_offset, total_num_frames));
    }
    if (num_threads < 1)
        throw std::invalid_argument("num_threads must be at least 1");

    const double inv_mean_over_num_frames =
        1.0 / static_cast<double>(mean_over_num_frames);

    NDArray<double, 3> cs_data({static_cast<ssize_t>(num_rows),
                                static_cast<ssize_t>(num_cols),
                                static_cast<ssize_t>(num_samples)},
                               0);

    auto worker = [&](const size_t sample_begin, const size_t sample_end) {
        std::vector<uint64_t> sum(
            num_rows * num_cols); // uint64_t to avoid overflow for uint16_t

        JungfrauDataFile reader(path);

        NDArray<uint16_t, 3> frames({static_cast<ssize_t>(frames_per_chunk),
                                     static_cast<ssize_t>(num_rows),
                                     static_cast<ssize_t>(num_cols)});

        for (size_t sample = sample_begin; sample < sample_end; ++sample) {
            std::fill(sum.begin(), sum.end(), 0);

            const size_t first = file_offset + sample * frames_per_sample;
            const size_t last = first + frames_per_sample;

            reader.seek(first);

            for (size_t frame_chunk = first; frame_chunk < last;
                 frame_chunk += frames_per_chunk) {

                const size_t n_frames_to_read =
                    std::min({frames_per_chunk, last - frame_chunk});

                reader.read_into(reinterpret_cast<std::byte *>(frames.data()),
                                 n_frames_to_read, nullptr);

                for (size_t f = 0; f < n_frames_to_read; ++f) {
                    // each frame illuminates every 64th column
                    const size_t illum_col_idx =
                        (frame_chunk + f - first) % columns_per_subcolumn;

                    const auto *data = reinterpret_cast<const uint16_t *>(
                        frames.data() + f * num_rows * num_cols);

                    for (size_t r = 0; r < half_rows; ++r) {
                        const size_t column_offset =
                            r * num_cols + illum_col_idx;
                        for (size_t s = 0; s < num_subcolumns; ++s) {
                            const size_t idx =
                                column_offset + s * columns_per_subcolumn;
                            sum[idx] += data[idx];
                        }
                    }
                    // top half modules are flipped
                    for (size_t r = half_rows; r < num_rows; ++r) {
                        const size_t column_offset = r * num_cols +
                                                     columns_per_subcolumn - 1 -
                                                     illum_col_idx;
                        for (size_t s = 0; s < num_subcolumns; ++s) {
                            const size_t idx =
                                column_offset + s * columns_per_subcolumn;
                            sum[idx] += data[idx];
                        }
                    }
                }
            }

            // uint16_t *out = cs_data.data() + sample * num_rows * num_cols;
            for (size_t row = 0; row < num_rows; ++row) {
                size_t row_offset = row * num_cols;
                for (size_t col = 0; col < num_cols; ++col) {
                    cs_data(row, col, sample) =
                        static_cast<double>(sum[row_offset + col]) *
                        inv_mean_over_num_frames; // bad striding maybe better
                                                  // to reshape and copy in
                                                  // numpy
                }
            }
        }
    };

    std::vector<std::future<void>> futures;
    futures.reserve(static_cast<size_t>(num_threads));

    const size_t samples_per_thread = num_samples / num_threads;

    for (int i = 0; i < num_threads; ++i) {
        size_t end_sample =
            (i == num_threads - 1) ? num_samples : (i + 1) * samples_per_thread;
        futures.push_back(std::async(std::launch::async, worker,
                                     i * samples_per_thread, end_sample));
    }

    for (auto &f : futures) {
        try {
            f.get();
        } catch (const std::exception &e) {
            throw std::runtime_error(std::string("Exception in thread: ") +
                                     e.what());
        }
    }

    return cs_data;
}

} // namespace aare
