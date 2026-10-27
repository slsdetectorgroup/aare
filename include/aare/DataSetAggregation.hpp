#pragma once
#include "aare/File.hpp"
#include "aare/MultiThreadedFileReader.hpp"
#include "aare/utils/math_helpers.hpp"
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
 * @brief Ring buffer of slots shared between one producer and a fixed number
 * of consumers. Item i is stored in slot i % size(). The producer can only
 * overwrite a slot once all consumers released the item stored in it, and a
 * consumer can only read item i once the producer committed it.
 * Calling abort() wakes up all waiting threads.
 */
template <typename ItemType> class BufferPool {
  public:
    /**
     * @param size number of slots in the pool
     * @param num_consumers number of consumers that read every item
     * @param initializer callable returning the initial ItemType of a slot
     */
    template <typename Func>
    BufferPool(size_t size, size_t num_consumers, Func initializer)
        : m_num_consumers(num_consumers), m_item_in_slot(size, no_item),
          m_pending_consumers(size, 0) {
        if (size == 0)
            throw std::invalid_argument("BufferPool size must be at least 1");
        m_buffers.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            m_buffers.emplace_back(initializer());
        }
    }

    /// @brief Get the number of slots in the buffer pool.
    /// @return The number of slots in the buffer pool.
    size_t size() const { return m_buffers.size(); }

    /**
     * @brief Block until the slot of item i is free (e.g. all consumers have
     * read it) to be written.
     * @return the buffer to write item i into or nullptr if aborted
     */
    ItemType *acquire_write(size_t i) {
        const size_t slot = i % size();
        std::unique_lock<std::mutex> lock(m_mtx);
        m_cv_free.wait(
            lock, [&] { return m_abort || m_pending_consumers[slot] == 0; });
        if (m_abort)
            return nullptr;
        return &m_buffers[slot];
    }

    /**
     * @brief Mark item i as written and wake up the consumers.
     */
    void commit_write(size_t i) {
        const size_t slot = i % size();
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            m_item_in_slot[slot] = i;
            m_pending_consumers[slot] = m_num_consumers;
        }
        m_cv_loaded.notify_all();
    }

    /**
     * @brief Block until item i has been written.
     * @return the buffer holding item i or nullptr if aborted
     */
    const ItemType *acquire_read(size_t i) {
        const size_t slot = i % size();
        std::unique_lock<std::mutex> lock(m_mtx);
        m_cv_loaded.wait(lock,
                         [&] { return m_abort || m_item_in_slot[slot] == i; });
        if (m_abort)
            return nullptr;
        return &m_buffers[slot];
    }

    /**
     * @brief Signal that this consumer no longer needs item i. Once all
     * consumers released it the slot can be reused by the producer.
     */
    void release_read(size_t i) {
        const size_t slot = i % size();
        bool slot_free = false;
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            slot_free = --m_pending_consumers[slot] == 0;
        }
        if (slot_free)
            m_cv_free.notify_one();
    }

    /**
     * @brief Wake up all waiting threads, acquire_* returns nullptr from now
     * on.
     */
    void abort() {
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            m_abort = true;
        }
        m_cv_loaded.notify_all();
        m_cv_free.notify_all();
    }

  private:
    size_t m_num_consumers;
    std::vector<ItemType> m_buffers;

    static constexpr size_t no_item = std::numeric_limits<size_t>::max();

    // per slot:
    /// @brief Index of the item currently committed to the slot.
    std::vector<size_t> m_item_in_slot; // store the item - boolean flag need to
                                        // set back to false ->
    std::vector<size_t> m_pending_consumers;

    std::mutex m_mtx;
    /// @brief Condition variable to notify consumers that an item has been
    /// loaded.
    std::condition_variable m_cv_loaded;
    /// @brief Condition variable to notify producers that a slot is free to
    /// overwrite.
    std::condition_variable m_cv_free;

    /// @brief Flag to indicate if the buffer pool should be aborted.
    bool m_abort{false};
};

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

    BufferPool<NDArray<FRAME_TYPE, 3>> buffers(
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
 * @param path The path to the Jungfrau detector file.
 * @param num_samples The number of samples in the dataset e.g. exposure
 times
 * @param mean_over_num_frames The number of frames to mean over for each
 sample.
 * @param num_threads The number of threads to use for parallel computation.
 * @param file_offset The offset in the file to start reading from.
 * @return An NDArray containing the mean current source data stored as
 double for each sample. (rows, cols, num_samples)

 */
NDArray<double, 3> mean_current_source(const std::filesystem::path &path,
                                       const size_t num_samples,
                                       const size_t mean_over_num_frames,
                                       const int num_threads = 16,
                                       size_t file_offset = 0);

} // namespace aare
