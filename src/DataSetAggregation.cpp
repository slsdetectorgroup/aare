// SPDX-License-Identifier: MPL-2.0
#include "aare/DataSetAggregation.hpp"
#include "aare/JungfrauDataFile.hpp"
#include "aare/MultiThreadedFileReader.hpp"
#include "aare/utils/math_helpers.hpp"

#include <cstdint>
#include <fmt/format.h>
#include <future>
#include <stdexcept>
#include <vector>

namespace aare {

// TODO: maybe O_DIRECT rea dis better to avoid the page cache

NDArray<double, 3> mean_current_source(const std::filesystem::path &path,
                                       const size_t num_samples,
                                       const size_t mean_over_num_frames,
                                       const size_t num_processing_threads,
                                       const size_t num_reader_threads,
                                       size_t file_offset) {

    constexpr size_t columns_per_subcolumn = 64;

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
    if (num_processing_threads < 1)
        throw std::invalid_argument(
            "num_processing_threads must be at least 1");
    if (num_reader_threads < 1)
        throw std::invalid_argument("num_reader_threads must be at least 1");

    const double inv_mean_over_num_frames =
        1.0 / static_cast<double>(mean_over_num_frames);

    NDArray<double, 3> cs_data({static_cast<ssize_t>(num_rows),
                                static_cast<ssize_t>(num_cols),
                                static_cast<ssize_t>(num_samples)},
                               0);

    constexpr size_t num_buffers = 4; // TODO: random for now reading much
                                      // slower - small buffer is enough

    BufferPool<NDArray<uint16_t, 3>> buffers(
        num_buffers, num_processing_threads,
        [&num_rows, &num_cols, &frames_per_sample] {
            return NDArray<uint16_t, 3>(
                std::array<ssize_t, 3>{static_cast<ssize_t>(frames_per_sample),
                                       static_cast<ssize_t>(num_rows),
                                       static_cast<ssize_t>(num_cols)});
        });

    auto reader = [&]() {
        try {
            // one wave of the reader covers exactly one batch
            experimental::MultiThreadedFileReader file_reader(
                path, static_cast<size_t>(num_reader_threads),
                ceil_div(frames_per_sample,
                         static_cast<size_t>(num_reader_threads)));

            file_reader.seek(file_offset);

            for (size_t b = 0; b < num_samples; ++b) {
                NDArray<uint16_t, 3> *buffer = buffers.acquire_write(b);
                if (!buffer)
                    throw std::runtime_error(
                        "Failed to acquire buffer for writing");

                file_reader.read_into(
                    reinterpret_cast<std::byte *>(buffer->data()),
                    frames_per_sample);

                buffers.commit_write(b);
            }
        } catch (...) {
            buffers.abort(); // wake up the other threads so they can exit
            throw;
        }
    };

    auto worker = [&](const size_t row_begin, const size_t row_end) {
        try {
            const size_t shard_size = (row_end - row_begin) * num_cols;
            // uint64_t to avoid overflow for uint16_t
            std::vector<uint64_t> sum(shard_size, 0);

            for (size_t sample = 0; sample < num_samples; ++sample) {
                const NDArray<uint16_t, 3> *buffer =
                    buffers.acquire_read(sample);
                if (!buffer)
                    throw std::runtime_error(
                        "Failed to acquire buffer for reading");

                for (size_t f = 0; f < frames_per_sample; ++f) {
                    // each frame illuminates every 64th column
                    const size_t illum_col_idx = f % columns_per_subcolumn;

                    const uint16_t *data =
                        buffer->data() + f * num_rows * num_cols;

                    for (size_t r = row_begin; r < row_end; ++r) {
                        // top half modules are flipped
                        const size_t col_in_subcolumn =
                            r < half_rows
                                ? illum_col_idx
                                : columns_per_subcolumn - 1 - illum_col_idx;
                        const size_t column_offset =
                            r * num_cols + col_in_subcolumn;
                        const size_t local_offset =
                            column_offset - row_begin * num_cols;
                        for (size_t s = 0; s < num_subcolumns; ++s) {
                            const size_t step = s * columns_per_subcolumn;
                            sum[local_offset + step] +=
                                data[column_offset + step];
                        }
                    }
                }

                // buffer slot is no longer needed, write output after release
                buffers.release_read(sample);

                for (size_t r = row_begin; r < row_end; ++r) {
                    const size_t local_row_offset = (r - row_begin) * num_cols;
                    for (size_t c = 0; c < num_cols; ++c) {
                        cs_data(r, c, sample) =
                            static_cast<double>(sum[local_row_offset + c]) *
                            inv_mean_over_num_frames;
                    }
                }
                std::fill(sum.begin(), sum.end(), 0);
            }
        } catch (...) {
            buffers.abort(); // wake up the other threads so they can exit
            throw;
        }
    };

    std::vector<std::future<void>> futures;
    futures.reserve(num_processing_threads + 1);

    // reader thread
    futures.push_back(std::async(std::launch::async, reader));

    const size_t rows_per_worker = num_rows / num_processing_threads;
    for (size_t w = 0; w < num_processing_threads; ++w) {
        const size_t row_end = (w == num_processing_threads - 1)
                                   ? num_rows
                                   : (w + 1) * rows_per_worker;
        futures.push_back(std::async(std::launch::async, worker,
                                     w * rows_per_worker, row_end));
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
