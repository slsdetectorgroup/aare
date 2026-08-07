// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "aare/Dtype.hpp"
#include "aare/File.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <vector>

namespace aare {

/**
 * @brief Read independent chunks of a file in parallel.
 *
 * Each worker opens its own File instance, so seeking and reading do not share
 * mutable file state. Chunks are written directly to their position in the
 * destination buffer and the resulting frame order is the same as in the file.
 */
class MultiThreadedFileReader {
  public:
    /**
     * @param fname path accepted by File
     * @param n_threads maximum number of worker threads
     * @param chunk_size number of frames claimed by a worker at a time
     * @param total_frames number of frames to read, or all frames when omitted
     */
    MultiThreadedFileReader(std::filesystem::path fname, size_t n_threads,
                            size_t chunk_size,
                            std::optional<size_t> total_frames = std::nullopt);

    MultiThreadedFileReader(const MultiThreadedFileReader &) = delete;
    MultiThreadedFileReader &
    operator=(const MultiThreadedFileReader &) = delete;
    MultiThreadedFileReader(MultiThreadedFileReader &&) noexcept = default;
    MultiThreadedFileReader &
    operator=(MultiThreadedFileReader &&) noexcept = default;

    /**
     * @brief Read one chunk per active worker into a caller-owned buffer.
     *
     * The buffer must hold at least next_read_bytes() bytes. The reader's
     * position advances by the returned number of frames. At the end of the
     * configured range this function returns zero and does not access the
     * destination.
     */
    size_t read_into(std::byte *destination);

    /** @brief Read the next wave of chunks into an owned byte buffer. */
    std::vector<std::byte> read();

    /** @brief Read every frame remaining from the current position. */
    std::vector<std::byte> read_all();

    /** @brief Set the next frame index to read. The end position is valid. */
    void seek(size_t frame_index);

    /** @brief Return the next frame index to read. */
    size_t tell() const noexcept { return m_current_frame; }

    /** @brief Close all worker files. Safe to call more than once. */
    void close() noexcept { m_files.clear(); }

    /** @brief Return whether the worker files are open. */
    bool is_open() const noexcept { return !m_files.empty(); }

    size_t n_threads() const noexcept { return m_n_threads; }
    size_t chunk_size() const noexcept { return m_chunk_size; }
    size_t total_frames() const noexcept { return m_total_frames; }
    size_t source_total_frames() const noexcept {
        return m_source_total_frames;
    }
    size_t rows() const noexcept { return m_rows; }
    size_t cols() const noexcept { return m_cols; }
    size_t bitdepth() const noexcept { return m_bitdepth; }
    Dtype dtype() const noexcept { return m_dtype; }
    size_t bytes_per_frame() const noexcept { return m_bytes_per_frame; }
    size_t total_bytes() const noexcept { return m_total_bytes; }
    size_t remaining_frames() const noexcept;
    size_t next_read_frames() const noexcept;
    size_t next_read_bytes() const noexcept {
        return next_read_frames() * m_bytes_per_frame;
    }

  private:
    std::filesystem::path m_fname;
    size_t m_n_threads;
    size_t m_chunk_size;
    size_t m_total_frames;
    size_t m_source_total_frames;
    size_t m_rows;
    size_t m_cols;
    size_t m_bitdepth;
    Dtype m_dtype;
    size_t m_bytes_per_frame;
    size_t m_total_bytes;
    size_t m_current_frame;
    std::vector<File> m_files;

    void ensure_open() const;
};

} // namespace aare
