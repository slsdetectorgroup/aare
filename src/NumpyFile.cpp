
#include "aare/NumpyFile.hpp"
#include "aare/NumpyHelpers.hpp"

namespace aare {



NumpyFile::NumpyFile(const std::filesystem::path &fname, const std::string &mode, FileConfig cfg) {
    // TODO! add opts to constructor

    m_mode = mode;
    if (mode == "r") {
        fp = fopen(fname.string().c_str(), "rb");
        if (!fp) {
            throw std::runtime_error(fmt::format("Could not open: {} for reading", fname.string()));
        }
        load_metadata();
    } else if (mode == "w") {
        m_bitdepth = cfg.dtype.bitdepth();
        m_rows = cfg.rows;
        m_cols = cfg.cols;
        m_header = {cfg.dtype, false, {cfg.rows, cfg.cols}};
        m_header.shape = {0, cfg.rows, cfg.cols};
        fp = fopen(fname.string().c_str(), "wb");
        if (!fp) {
            throw std::runtime_error(fmt::format("Could not open: {} for reading", fname.string()));
        }
        initial_header_len = aare::NumpyHelpers::write_header(std::filesystem::path(fname.c_str()), m_header);
    }
    m_pixels_per_frame = std::accumulate(m_header.shape.begin() + 1, m_header.shape.end(), 1, std::multiplies<>());

    m_bytes_per_frame = m_header.dtype.bitdepth() / 8 * m_pixels_per_frame;
}
void NumpyFile::write(Frame &frame) { write_impl(frame.data(), frame.bytes()); }
void NumpyFile::write_impl(void *data, uint64_t size) {

    if (fp == nullptr) {
        throw std::runtime_error("File not open");
    }
    if (!(m_mode == "w" || m_mode == "a")) {
        throw std::invalid_argument("File not open for writing");
    }
    if (fseek(fp, 0, SEEK_END))
        throw std::runtime_error("Could not seek to end of file");
    size_t const rc = fwrite(data, size, 1, fp);
    if (rc != 1) {
        throw std::runtime_error("Error writing frame to file");
    }

    m_header.shape[0]++;
}

Frame NumpyFile::get_frame(size_t frame_number) {
    Frame frame(m_header.shape[1], m_header.shape[2], m_header.dtype);
    get_frame_into(frame_number, frame.data());
    return frame;
}
void NumpyFile::get_frame_into(size_t frame_number, std::byte *image_buf) {
    if (fp == nullptr) {
        throw std::runtime_error("File not open");
    }
    if (frame_number > m_header.shape[0]) {
        throw std::invalid_argument("Frame number out of range");
    }
    if (fseek(fp, header_size + frame_number * m_bytes_per_frame, SEEK_SET)) // NOLINT
        throw std::runtime_error("Could not seek to frame");

    size_t const rc = fread(image_buf, m_bytes_per_frame, 1, fp);
    if (rc != 1) {
        throw std::runtime_error("Error reading frame from file");
    }
}

size_t NumpyFile::pixels_per_frame() { return m_pixels_per_frame; };
size_t NumpyFile::bytes_per_frame() { return m_bytes_per_frame; };

std::vector<Frame> NumpyFile::read_n(size_t n_frames) {
    // TODO: implement this in a more efficient way
    std::vector<Frame> frames;
    for (size_t i = 0; i < n_frames; i++) {
        frames.push_back(get_frame(current_frame));
        current_frame++;
    }
    return frames;
}
void NumpyFile::read_into(std::byte *image_buf, size_t n_frames) {
    // TODO: implement this in a more efficient way
    for (size_t i = 0; i < n_frames; i++) {
        get_frame_into(current_frame++, image_buf);
        image_buf += m_bytes_per_frame;
    }
}

NumpyFile::~NumpyFile() noexcept {
    if (m_mode == "w" || m_mode == "a") {
        // determine number of frames
        if (fseek(fp, 0, SEEK_END)) {
            std::cout << "Could not seek to end of file" << std::endl;
        }
        size_t const file_size = ftell(fp);
        size_t const data_size = file_size - initial_header_len;
        size_t const n_frames = data_size / m_bytes_per_frame;
        // update number of frames in header (first element of shape)
        m_header.shape[0] = n_frames;
        if (fseek(fp, 0, SEEK_SET)) {
            std::cout << "Could not seek to beginning of file" << std::endl;
        }
        // create string stream to contain header
        std::stringstream ss;
        aare::NumpyHelpers::write_header(ss, m_header);
        std::string const header_str = ss.str();
        // write header
        size_t const rc = fwrite(header_str.c_str(), header_str.size(), 1, fp);
        if (rc != 1) {
            std::cout << "Error writing header to numpy file in destructor" << std::endl;
        }
    }

    if (fp != nullptr) {
        if (fclose(fp)) {
            std::cout << "Error closing file" << std::endl;
        }
    }
}

void NumpyFile::load_metadata() {

    // read magic number
    std::array<char, 6> tmp{};
    size_t rc = fread(tmp.data(), tmp.size(), 1, fp);
    if (rc != 1) {
        throw std::runtime_error("Error reading magic number");
    }
    if (tmp != aare::NumpyHelpers::magic_str) {
        for (auto item : tmp)
            fmt::print("{}, ", static_cast<int>(item));
        fmt::print("\n");
        throw std::runtime_error("Not a numpy file");
    }

    // read version
    rc = fread(reinterpret_cast<char *>(&major_ver_), sizeof(major_ver_), 1, fp);
    rc += fread(reinterpret_cast<char *>(&minor_ver_), sizeof(minor_ver_), 1, fp);
    if (rc != 2) {
        throw std::runtime_error("Error reading numpy version");
    }

    if (major_ver_ == 1) {
        header_len_size = 2;
    } else if (major_ver_ == 2) {
        header_len_size = 4;
    } else {
        throw std::runtime_error("Unsupported numpy version");
    }

    // read header length
    rc = fread(reinterpret_cast<char *>(&header_len), header_len_size, 1, fp);
    if (rc != 1) {
        throw std::runtime_error("Error reading header length");
    }
    header_size = aare::NumpyHelpers::magic_string_length + 2 + header_len_size + header_len;
    if (header_size % 16 != 0) {
        fmt::print("Warning: header length is not a multiple of 16\n");
    }

    // read header
    std::string header(header_len, '\0');
    rc = fread(header.data(), header_len, 1, fp);
    if (rc != 1) {
        throw std::runtime_error("Error reading header");
    }

    // parse header
    std::vector<std::string> const keys{"descr", "fortran_order", "shape"};
    auto dict_map = aare::NumpyHelpers::parse_dict(header, keys);
    if (dict_map.empty())
        throw std::runtime_error("invalid dictionary in header");

    std::string const descr_s = dict_map["descr"];
    std::string const fortran_s = dict_map["fortran_order"];
    std::string const shape_s = dict_map["shape"];

    std::string const descr = aare::NumpyHelpers::parse_str(descr_s);
    aare::Dtype const dtype = aare::NumpyHelpers::parse_descr(descr);

    // convert literal Python bool to C++ bool
    bool const fortran_order = aare::NumpyHelpers::parse_bool(fortran_s);

    // parse the shape tuple
    auto shape_v = aare::NumpyHelpers::parse_tuple(shape_s);
    std::vector<size_t> shape;
    for (const auto &item : shape_v) {
        auto dim = static_cast<size_t>(std::stoul(item));
        shape.push_back(dim);
    }
    m_header = {dtype, fortran_order, shape};
}

} // namespace aare