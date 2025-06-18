#include "aare/CustomFiles.hpp"

namespace aare {

CustomMythenFile::CustomMythenFile(const std::string &filename,
                                   const ssize_t rows, const ssize_t cols,
                                   const std::string &mode)
    : m_filename(filename), m_rows(rows), m_cols(cols) {

    m_mode = mode;
    if (m_mode == "r") {
        try {
            m_file.open(m_filename, std::ios_base::in);
            if (!m_file.good()) {
                throw std::logic_error("file does not exist");
            }
        } catch (std::exception &e) {

            std::cerr << "Error: " << e.what()
                      << std::endl; // TODO replace with log
        }

    } else {
        throw std::runtime_error(LOCATION +
                                 "Unsupported mode. Can only read RawFiles.");
    }
}

CustomMythenFile::~CustomMythenFile() { m_file.close(); }

Frame CustomMythenFile::read_frame() {
    auto f = Frame(m_rows, m_cols, m_dtype);
    uint32_t *frame_buffer = reinterpret_cast<uint32_t *>(f.data());
    uint32_t strip_index, photon_count;
    while (m_file >> strip_index >> photon_count) {
        *frame_buffer = photon_count;
        ++frame_buffer;
    }
    return f;
}

void CustomMythenFile::read_into(std::byte *image_buf) {
    uint32_t strip_index, photon_count;
    while (m_file >> strip_index >> photon_count) {
        std::memcpy(image_buf, &photon_count, sizeof(photon_count));
        image_buf += sizeof(photon_count);
    }
}

size_t CustomMythenFile::bytes_per_frame() {
    return m_num_strips * m_dtype.bytes(); // TODO do i want m_counts?
}

Frame CustomMythenFile::read_frame(size_t frame_number) {
    return read_frame(); // maybe give count as frame_number
}

std::vector<Frame> CustomMythenFile::read_n(size_t n_frames) {
    std::vector<Frame> vec;
    vec.reserve(1);
    vec.push_back(read_frame());
    return vec; // std::vector<Frame>{read_frame()};
}

void CustomMythenFile::read_into(std::byte *image_buf, size_t n_frames) {
    read_into(image_buf);
}

size_t CustomMythenFile::frame_number(size_t frame_index) { return 1; }

size_t CustomMythenFile::pixels_per_frame() { return m_rows * m_cols; }

void CustomMythenFile::seek(size_t frame_number) {}

size_t CustomMythenFile::tell() { return 1; }

size_t CustomMythenFile::total_frames() const { return 1; }

size_t CustomMythenFile::rows() const { return m_rows; }

size_t CustomMythenFile::cols() const { return m_cols; }

size_t CustomMythenFile::bitdepth() const { return m_dtype.bitdepth(); }

DetectorType CustomMythenFile::detector_type() const { return det_type; }

const Dtype CustomMythenFile::m_dtype(Dtype::TypeIndex::UINT32);

} // namespace aare