#include "aare/JungfrauDataFile.hpp"
#include "aare/algorithm.hpp"
#include "aare/defs.hpp"

#include <cerrno>
#include <fmt/format.h>

namespace aare {

JungfrauDataFile::JungfrauDataFile(const std::filesystem::path &fname) {

    if (!std::filesystem::exists(fname)) {
        throw std::runtime_error(LOCATION +
                                 "File does not exist: " + fname.string());
    }
    find_frame_size(fname);
    parse_fname(fname);
    scan_files();
    open_file(m_current_file_index);
}

// FileInterface

Frame JungfrauDataFile::read_frame() {
    Frame f(rows(), cols(), Dtype::UINT16);
    read_into(reinterpret_cast<std::byte *>(f.data()), nullptr);
    return f;
}

Frame JungfrauDataFile::read_frame(size_t frame_number) {
    seek(frame_number);
    Frame f(rows(), cols(), Dtype::UINT16);
    read_into(reinterpret_cast<std::byte *>(f.data()), nullptr);
    return f;
}

std::vector<Frame> JungfrauDataFile::read_n(size_t n_frames) {
    std::vector<Frame> frames;
    for (size_t i = 0; i < n_frames; ++i) {
        frames.push_back(read_frame());
    }
    return frames;
}

void JungfrauDataFile::read_into(std::byte *image_buf) {
    read_into(image_buf, nullptr);
}
void JungfrauDataFile::read_into(std::byte *image_buf, size_t n_frames) {
    read_into(image_buf, n_frames, nullptr);
}

size_t JungfrauDataFile::frame_number(size_t frame_index) {
    seek(frame_index);
    return read_header().framenum;
}

std::array<ssize_t, 2> JungfrauDataFile::shape() const {
    return {static_cast<ssize_t>(rows()), static_cast<ssize_t>(cols())};
}

DetectorType JungfrauDataFile::detector_type() const {
    return DetectorType::Jungfrau;
}

std::string JungfrauDataFile::base_name() const { return m_base_name; }

size_t JungfrauDataFile::bytes_per_frame() { return m_bytes_per_frame; }

size_t JungfrauDataFile::pixels_per_frame() { return m_rows * m_cols; }

size_t JungfrauDataFile::bytes_per_pixel() const { return sizeof(pixel_type); }

size_t JungfrauDataFile::bitdepth() const {
    return bytes_per_pixel() * bits_per_byte;
}

void JungfrauDataFile::seek(size_t frame_index) {
    if (frame_index >= m_total_frames) {
        throw std::runtime_error(LOCATION + "Frame index out of range: " +
                                 std::to_string(frame_index));
    }
    m_current_frame_index = frame_index;
    auto file_index = first_larger(m_last_frame_in_file, frame_index);

    if (file_index != m_current_file_index)
        open_file(file_index);

    auto frame_offset = (file_index)
                            ? frame_index - m_last_frame_in_file[file_index - 1]
                            : frame_index;
    auto byte_offset = frame_offset * (m_bytes_per_frame + header_size);
    m_fp.seek(byte_offset);
}

size_t JungfrauDataFile::tell() { return m_current_frame_index; }
size_t JungfrauDataFile::total_frames() const { return m_total_frames; }
size_t JungfrauDataFile::rows() const { return m_rows; }
size_t JungfrauDataFile::cols() const { return m_cols; }

size_t JungfrauDataFile::n_files() const { return m_last_frame_in_file.size(); }

void JungfrauDataFile::find_frame_size(const std::filesystem::path &fname) {

    static constexpr size_t module_data_size =
        header_size + sizeof(pixel_type) * 512 * 1024;
    static constexpr size_t half_data_size =
        header_size + sizeof(pixel_type) * 256 * 1024;
    static constexpr size_t chip_data_size =
        header_size + sizeof(pixel_type) * 256 * 256;

    auto file_size = std::filesystem::file_size(fname);
    if (file_size == 0) {
        throw std::runtime_error(LOCATION +
                                 "Cannot guess frame size: file is empty");
    }

    if (file_size % module_data_size == 0) {
        m_rows = 512;
        m_cols = 1024;
        m_bytes_per_frame = module_data_size - header_size;
    } else if (file_size % half_data_size == 0) {
        m_rows = 256;
        m_cols = 1024;
        m_bytes_per_frame = half_data_size - header_size;
    } else if (file_size % chip_data_size == 0) {
        m_rows = 256;
        m_cols = 256;
        m_bytes_per_frame = chip_data_size - header_size;
    } else {
        throw std::runtime_error(LOCATION +
                                 "Cannot find frame size: file size is not a "
                                 "multiple of any known frame size");
    }
}

void JungfrauDataFile::parse_fname(const std::filesystem::path &fname) {
    m_path = fname.parent_path();
    m_base_name = fname.stem();

    // find file index, then remove if from the base name
    if (auto pos = m_base_name.find_last_of('_'); pos != std::string::npos) {
        m_offset = std::stoul(m_base_name.substr(pos + 1));
        m_base_name.erase(pos);
    }
}

void JungfrauDataFile::scan_files() {
    // find how many files we have and the number of frames in each file
    m_last_frame_in_file.clear();
    size_t file_index = m_offset;
    while (std::filesystem::exists(fpath(file_index))) {
        auto n_frames = std::filesystem::file_size(fpath(file_index)) /
                        (m_bytes_per_frame + header_size);
        m_last_frame_in_file.push_back(n_frames);
        ++file_index;
    }

    // find where we need to open the next file and total number of frames
    m_last_frame_in_file = cumsum(m_last_frame_in_file);
    m_total_frames = m_last_frame_in_file.back();
}

void JungfrauDataFile::read_into(std::byte *image_buf,
                                 JungfrauDataHeader *header) {

    // read header if not passed nullptr
    if (header) {
        if (auto rc = fread(header, sizeof(JungfrauDataHeader), 1, m_fp.get());
            rc != 1) {
            throw std::runtime_error(
                LOCATION +
                "Could not read header from file:" + m_fp.error_msg());
        }
    } else {
        m_fp.seek(header_size, SEEK_CUR);
    }

    // read data
    if (auto rc = fread(image_buf, 1, m_bytes_per_frame, m_fp.get());
        rc != m_bytes_per_frame) {
        throw std::runtime_error(LOCATION + "Could not read image from file" +
                                 m_fp.error_msg());
    }

    // prepare for next read
    // if we are at the end of the file, open the next file
    ++m_current_frame_index;
    if (m_current_frame_index >= m_last_frame_in_file[m_current_file_index] &&
        (m_current_frame_index < m_total_frames)) {
        ++m_current_file_index;
        open_file(m_current_file_index);
    }
}

void JungfrauDataFile::read_into(std::byte *image_buf, size_t n_frames,
                                 JungfrauDataHeader *header) {
    if (header) {
        for (size_t i = 0; i < n_frames; ++i)
            read_into(image_buf + i * m_bytes_per_frame, header + i);
    } else {
        for (size_t i = 0; i < n_frames; ++i)
            read_into(image_buf + i * m_bytes_per_frame, nullptr);
    }
}

void JungfrauDataFile::read_into(NDArray<uint16_t> *image,
                                 JungfrauDataHeader *header) {
    if (image->shape() != shape()) {
        throw std::runtime_error(
            LOCATION + "Image shape does not match file size: " +
            std::to_string(rows()) + "x" + std::to_string(cols()));
    }
    read_into(reinterpret_cast<std::byte *>(image->data()), header);
}

JungfrauDataHeader JungfrauDataFile::read_header() {
    JungfrauDataHeader header;
    if (auto rc = fread(&header, 1, sizeof(header), m_fp.get());
        rc != sizeof(header)) {
        throw std::runtime_error(LOCATION + "Could not read header from file" +
                                 m_fp.error_msg());
    }
    m_fp.seek(-header_size, SEEK_CUR);
    return header;
}

void JungfrauDataFile::open_file(size_t file_index) {
    // fmt::print(stderr, "Opening file: {}\n",
    // fpath(file_index+m_offset).string());
    m_fp = FilePtr(fpath(file_index + m_offset), "rb");
    m_current_file_index = file_index;
}

std::filesystem::path JungfrauDataFile::fpath(size_t file_index) const {
    auto fname = fmt::format("{}_{:0{}}.dat", m_base_name, file_index,
                             n_digits_in_file_index);
    return m_path / fname;
}

} // namespace aare
