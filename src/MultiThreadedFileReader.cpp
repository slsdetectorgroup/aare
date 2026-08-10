// SPDX-License-Identifier: MPL-2.0
#include "aare/MultiThreadedFileReader.hpp"

#include "aare/File.hpp"

#include <algorithm>
#include <future>
#include <limits>
#include <stdexcept>
#include <utility>

namespace aare::experimental {
namespace {

size_t checked_product(size_t lhs, size_t rhs) {
    if (lhs != 0 && rhs > std::numeric_limits<size_t>::max() / lhs) {
        throw std::overflow_error(
            "MultiThreadedFileReader buffer size overflow");
    }
    return lhs * rhs;
}

} // namespace

MultiThreadedFileReader::MultiThreadedFileReader(
    std::filesystem::path fname, size_t n_threads, size_t chunk_size,
    std::optional<size_t> total_frames)
    : m_fname(std::move(fname)), m_n_threads(n_threads),
      m_chunk_size(chunk_size), m_total_frames(0), m_source_total_frames(0),
      m_rows(0), m_cols(0), m_bitdepth(0), m_dtype(Dtype::NONE),
      m_bytes_per_frame(0), m_total_bytes(0), m_current_frame(0) {
    if (m_n_threads == 0) {
        throw std::invalid_argument(
            "MultiThreadedFileReader requires at least one thread");
    }
    if (m_chunk_size == 0) {
        throw std::invalid_argument(
            "MultiThreadedFileReader chunk size must be greater than zero");
    }

    File file(m_fname);
    m_source_total_frames = file.total_frames();
    m_total_frames = total_frames.value_or(m_source_total_frames);
    if (m_total_frames > m_source_total_frames) {
        throw std::invalid_argument(
            "Requested frame count exceeds the number of frames in the file");
    }

    m_rows = file.rows();
    m_cols = file.cols();
    m_bitdepth = file.bitdepth();
    m_dtype = file.dtype();
    m_bytes_per_frame = file.bytes_per_frame();
    m_total_bytes = checked_product(m_total_frames, m_bytes_per_frame);

    m_files.reserve(m_n_threads);
    m_files.push_back(std::move(file));
    for (size_t i = 1; i < m_n_threads; ++i) {
        m_files.emplace_back(m_fname);
    }
}

size_t MultiThreadedFileReader::remaining_frames() const noexcept {
    return m_total_frames - m_current_frame;
}

size_t MultiThreadedFileReader::next_read_frames() const noexcept {
    const size_t remaining = remaining_frames();
    if (remaining == 0) {
        return 0;
    }

    const size_t chunks_remaining = 1 + (remaining - 1) / m_chunk_size;
    if (m_n_threads >= chunks_remaining) {
        return remaining;
    }
    // This multiplication is safe: in this branch n_threads * chunk_size is
    // strictly smaller than remaining.
    return m_n_threads * m_chunk_size;
}

void MultiThreadedFileReader::ensure_open() const {
    if (!is_open()) {
        throw std::runtime_error("MultiThreadedFileReader is closed");
    }
}

size_t MultiThreadedFileReader::read_into(std::byte *destination) {
    ensure_open();
    const size_t frames_to_read = next_read_frames();
    if (frames_to_read == 0) {
        return 0;
    }
    if (destination == nullptr) {
        throw std::invalid_argument(
            "MultiThreadedFileReader destination must not be null");
    }

    const size_t first_frame = m_current_frame;
    const size_t active_threads = 1 + (frames_to_read - 1) / m_chunk_size;

    auto worker = [&](size_t worker_index) {
        File &file = m_files[worker_index];
        const size_t batch_offset = worker_index * m_chunk_size;
        const size_t begin = first_frame + batch_offset;
        const size_t count =
            std::min(m_chunk_size, frames_to_read - batch_offset);
        file.seek(begin);
        file.read_into(destination + batch_offset * m_bytes_per_frame, count);
    };

    std::vector<std::future<void>> workers;
    workers.reserve(active_threads);
    for (size_t i = 0; i < active_threads; ++i) {
        workers.emplace_back(std::async(std::launch::async, worker, i));
    }

    for (auto &future : workers) {
        future.get();
    }
    m_current_frame += frames_to_read;
    return frames_to_read;
}

std::vector<std::byte> MultiThreadedFileReader::read() {
    std::vector<std::byte> data(next_read_bytes());
    read_into(data.data());
    return data;
}

std::vector<std::byte> MultiThreadedFileReader::read_all() {
    ensure_open();
    std::vector<std::byte> data(
        checked_product(remaining_frames(), m_bytes_per_frame));
    size_t offset = 0;
    while (remaining_frames() != 0) {
        const size_t frames_read = read_into(data.data() + offset);
        offset += frames_read * m_bytes_per_frame;
    }
    return data;
}

void MultiThreadedFileReader::seek(size_t frame_index) {
    ensure_open();
    if (frame_index > m_total_frames) {
        throw std::out_of_range(
            "MultiThreadedFileReader frame index is out of range");
    }
    m_current_frame = frame_index;
}

} // namespace aare::experimental
