#pragma once
#include "aare/File.hpp"
#include <algorithm>
#include <array>
#include <condition_variable>
#include <filesystem>
#include <future>
#include <limits>
#include <mutex>
#include <vector>

namespace aare {

/**
 * @brief Compute the block mean over several frames per sample stored in a file
 * @tparam FRAME_TYPE The type of the frames in the dataset.
 * @param path The path to the file containing the dataset.
 * @param num_samples The number of samples in the dataset.
 * @param mean_over_num_frames The number of frames per sample to take mean
 * over.
 * @param num_threads The number of threads to use for parallel computation.
 * One thread reads the frames, the remaining threads each process a shard of
 * rows.
 * @return An NDArray containing the mean frame stored as double for each
 * sample.
 */
template <typename FRAME_TYPE>
NDArray<double, 3>
block_mean(const std::filesystem::path &path, const size_t num_samples,
           const size_t mean_over_num_frames, const int num_threads = 16) {

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

    if (num_threads < 1)
        throw std::invalid_argument("num_threads must be at least 1");

    if (total_num_frames != num_samples * mean_over_num_frames) {
        throw std::runtime_error(
            fmt::format("Frames mismatch: Expected {} frames got {}",
                        num_samples * mean_over_num_frames, total_num_frames));
    }

    const double inv_n = 1.0 / static_cast<double>(mean_over_num_frames);

    // One reader thread loads all frames of a sample into a ring buffer slot,
    // while the worker threads each compute the mean for their own shard of
    // rows. Reading the next samples overlaps with processing the current one.
    constexpr size_t num_buffers = 3;

    const size_t num_workers =
        std::min(std::max<size_t>(1, static_cast<size_t>(num_threads) - 1),
                 std::max<size_t>(1, rows));

    std::vector<NDArray<FRAME_TYPE, 3>> buffers;
    buffers.reserve(num_buffers);
    for (size_t b = 0; b < num_buffers; ++b) {
        buffers.emplace_back(std::array<ssize_t, 3>{
            static_cast<ssize_t>(mean_over_num_frames),
            static_cast<ssize_t>(rows), static_cast<ssize_t>(cols)});
    }

    // per slot: sample currently loaded and number of workers still using it
    constexpr size_t no_sample = std::numeric_limits<size_t>::max();
    std::vector<size_t> loaded_sample(num_buffers, no_sample);
    std::vector<size_t> pending_workers(num_buffers, 0);
    bool abort = false;

    std::mutex mtx;
    std::condition_variable cv_loaded;
    std::condition_variable cv_free;

    auto set_abort = [&]() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            abort = true;
        }
        cv_loaded.notify_all();
        cv_free.notify_all();
    };

    auto reader = [&]() {
        try {
            File file(path);
            for (size_t i = 0; i < num_samples; ++i) {
                const size_t slot = i % num_buffers;
                {
                    std::unique_lock<std::mutex> lock(mtx);
                    cv_free.wait(lock, [&] {
                        return abort || pending_workers[slot] == 0;
                    });
                    if (abort)
                        return;
                }

                file.read_into(
                    reinterpret_cast<std::byte *>(buffers[slot].data()),
                    mean_over_num_frames);

                {
                    std::lock_guard<std::mutex> lock(mtx);
                    loaded_sample[slot] = i;
                    pending_workers[slot] = num_workers;
                }
                cv_loaded.notify_all();
            }
        } catch (...) {
            set_abort();
            throw;
        }
    };

    auto worker = [&](size_t row_begin, size_t row_end) {
        try {
            const size_t shard_size = (row_end - row_begin) * cols;
            // need to be double to avoid overflow for uint16_t and uint32_t
            std::vector<double> sum(shard_size);

            for (size_t i = 0; i < num_samples; ++i) {
                const size_t slot = i % num_buffers;
                {
                    std::unique_lock<std::mutex> lock(mtx);
                    cv_loaded.wait(lock, [&] {
                        return abort || loaded_sample[slot] == i;
                    });
                    if (abort)
                        return;
                }

                std::fill(sum.begin(), sum.end(), 0.0);

                const FRAME_TYPE *src = buffers[slot].data() + row_begin * cols;
                for (size_t f = 0; f < mean_over_num_frames; ++f) {
                    const FRAME_TYPE *frame = src + f * rows * cols;
                    for (size_t k = 0; k < shard_size; ++k) {
                        sum[k] += static_cast<double>(frame[k]);
                    }
                }

                bool slot_free = false;
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    slot_free = --pending_workers[slot] == 0;
                }
                if (slot_free)
                    cv_free.notify_one();

                // buffer slot is no longer needed, write output after release
                for (size_t r = row_begin; r < row_end; ++r) {
                    for (size_t c = 0; c < cols; ++c) {
                        mean_per_sample(r, c, i) =
                            sum[(r - row_begin) * cols + c] * inv_n;
                    }
                }
            }
        } catch (...) {
            set_abort();
            throw;
        }
    };

    std::vector<std::future<void>> futures;
    futures.reserve(num_workers + 1);

    futures.push_back(std::async(std::launch::async, reader));

    const size_t rows_per_worker = rows / num_workers;
    for (size_t w = 0; w < num_workers; ++w) {
        size_t row_end =
            (w == num_workers - 1) ? rows : (w + 1) * rows_per_worker;
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
 * @param path The path to the Jungfrau detector file.
 * @param num_samples The number of samples in the dataset e.g. exposure times
 * @param mean_over_num_frames The number of frames to mean over for each
 sample.
 * @param num_threads The number of threads to use for parallel computation.
 * @param file_offset The offset in the file to start reading from.
 * @return An NDArray containing the mean current source data stored as double
 for each sample. (rows, cols, num_samples)

 */
NDArray<double, 3> mean_current_source(const std::filesystem::path &path,
                                       const size_t num_samples,
                                       const size_t mean_over_num_frames,
                                       const int num_threads = 16,
                                       size_t file_offset = 0);

} // namespace aare
