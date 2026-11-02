#pragma once
#include "aare/File.hpp"
#include "aare/MultiThreadedFileReader.hpp"
#include "aare/utils/RingBufferPool.hpp"
#include "aare/utils/math_helpers.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <future>
#include <vector>

namespace aare {

/**
 * @brief Compute the block mean over several frames per sample stored in a
 * file. Function spawns num_processing_threads + num_reader_threads + 1 threads
 * in total.
 * @tparam FRAME_TYPE The type of the frames in the dataset.
 * @param path The path to the file containing the dataset.
 * @param num_samples The number of samples in the dataset.
 * @param mean_over_num_frames The number of frames per sample to take mean
 * over.
 * @param num_processing_threads The number of threads to use for parallel
 * computation. One thread reads the frames, the remaining threads each process
 * a shard of rows. (default 12)
 * @param num_reader_threads The number of file workers the reader thread
 * uses to load the frames of one sample in parallel. (default 8)
 * @return An NDArray containing the mean frame stored as double for each
 * sample.
 */
template <typename FRAME_TYPE>
NDArray<double, 3> block_mean(const std::filesystem::path &path,
                              const size_t num_samples,
                              const size_t mean_over_num_frames,
                              const size_t num_processing_threads = 12,
                              const size_t num_reader_threads = 8) {

    size_t rows = 0;
    size_t cols = 0;
    size_t total_num_frames = 0;

    {
        File meta_file(path);
        rows = meta_file.rows();
        cols = meta_file.cols();
        total_num_frames = meta_file.total_frames();
    }

    NDArray<double, 3> mean_per_sample({static_cast<ssize_t>(rows),
                                        static_cast<ssize_t>(cols),
                                        static_cast<ssize_t>(num_samples)});

    if (num_processing_threads < 1)
        throw std::invalid_argument(
            "num_processing_threads must be at least 1");
    if (num_reader_threads < 1)
        throw std::invalid_argument("num_reader_threads must be at least 1");

    if (total_num_frames != num_samples * mean_over_num_frames) {
        throw std::runtime_error(
            fmt::format("Frames mismatch: Expected {} frames got {}",
                        num_samples * mean_over_num_frames, total_num_frames));
    }

    const double inv_n = 1.0 / static_cast<double>(mean_over_num_frames);

    constexpr size_t num_buffers =
        4; // TODO: random for now reading much slower - small buffer is enough

    RingBufferPool<NDArray<FRAME_TYPE, 3>> buffers(
        num_buffers, num_processing_threads,
        [&mean_over_num_frames, &rows, &cols] {
            return NDArray<FRAME_TYPE, 3>(std::array<ssize_t, 3>{
                static_cast<ssize_t>(mean_over_num_frames),
                static_cast<ssize_t>(rows), static_cast<ssize_t>(cols)});
        });

    auto reader = [&]() {
        try {
            // one wave of the reader covers exactly one sample
            experimental::MultiThreadedFileReader file_reader(
                path, num_reader_threads,
                ceil_div(mean_over_num_frames, num_reader_threads));

            for (size_t i = 0; i < num_samples; ++i) {
                NDArray<FRAME_TYPE, 3> *buffer = buffers.acquire_write(i);
                if (!buffer)
                    throw std::runtime_error(
                        "Failed to acquire buffer for writing");

                file_reader.read_into(
                    reinterpret_cast<std::byte *>(buffer->data()),
                    mean_over_num_frames);

                buffers.commit_write(i);
            }
        } catch (...) {
            buffers.abort(); // wake up the other threads so they can exit
            throw;
        }
    };

    auto worker = [&](size_t row_begin, size_t row_end) {
        try {
            const size_t shard_size = (row_end - row_begin) * cols;
            // need to be double to avoid overflow for uint16_t and uint32_t
            std::vector<double> sum(shard_size);

            for (size_t i = 0; i < num_samples; ++i) {
                const NDArray<FRAME_TYPE, 3> *buffer = buffers.acquire_read(i);
                if (!buffer)
                    throw std::runtime_error(
                        "Failed to acquire buffer for reading");

                std::fill(sum.begin(), sum.end(), 0.0);

                const FRAME_TYPE *src = buffer->data() + row_begin * cols;
                for (size_t f = 0; f < mean_over_num_frames; ++f) {
                    const FRAME_TYPE *frame = src + f * rows * cols;
                    for (size_t k = 0; k < shard_size; ++k) {
                        sum[k] += static_cast<double>(frame[k]);
                    }
                }

                // buffer slot is no longer needed, write output after
                // release
                buffers.release_read(i);

                for (size_t r = row_begin; r < row_end; ++r) {
                    size_t local_row = r - row_begin;
                    for (size_t c = 0; c < cols; ++c) {
                        mean_per_sample(r, c, i) =
                            sum[local_row * cols + c] * inv_n;
                    }
                }
            }
        } catch (...) {
            buffers.abort(); // wake up the other threads so they can exit
            throw;
        }
    };

    std::vector<std::future<void>> futures;
    futures.reserve(num_processing_threads + 1);

    futures.push_back(std::async(std::launch::async, reader));

    const size_t rows_per_worker = rows / num_processing_threads;
    for (size_t w = 0; w < num_processing_threads; ++w) {
        size_t row_end = (w == num_processing_threads - 1)
                             ? rows
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

    return mean_per_sample;
}

/**
 * @brief Compute the mean current source data from a Jungfrau detector file
 * Spawns num_processing_threads + num_reader_threads + 1 threads in total.
 * @param path The path to the Jungfrau detector file.
 * @param num_samples The number of samples in the dataset e.g. exposure
 times
 * @param mean_over_num_frames The number of frames to mean over for each
 sample.
 * @param num_processing_threads The number of threads to use for parallel
 computation. (default 12)
 * @param num_reader_threads The number of threads to use for reading data.
 (default 8)
 * @param file_offset The offset in the file to start reading from.
 * @return An NDArray containing the mean current source data stored as
 double for each sample. (rows, cols, num_samples)

 */
NDArray<double, 3> mean_current_source(const std::filesystem::path &path,
                                       const size_t num_samples,
                                       const size_t mean_over_num_frames,
                                       const size_t num_processing_threads = 12,
                                       const size_t num_reader_threads = 8,
                                       size_t file_offset = 0);

} // namespace aare
