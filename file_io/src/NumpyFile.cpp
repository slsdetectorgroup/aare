
#include "aare/NumpyFile.hpp"

namespace aare{

NumpyFile::NumpyFile(const std::filesystem::path& fname) {
    //TODO! add opts to constructor
    m_fname = fname;
    fp = fopen(m_fname.c_str(), "rb");
    if (!fp) {
        throw std::runtime_error(fmt::format("Could not open: {} for reading", m_fname.c_str()));
    }
    load_metadata();
}
NumpyFile::NumpyFile(FileConfig config, header_t header) {
    mode = "w";
    m_fname = config.fname;
    m_bitdepth = config.dtype.bitdepth();
    m_rows = config.rows;
    m_cols = config.cols;
    m_header = header;
    m_header.shape = {0, config.rows, config.cols};

    fp = fopen(m_fname.c_str(), "wb");
    if (!fp) {
        throw std::runtime_error(fmt::format("Could not open: {} for reading", m_fname.c_str()));
    }

    initial_header_len =
        aare::NumpyHelpers::write_header(std::filesystem::path(m_fname.c_str()), header);
}

void NumpyFile::write(Frame &frame) {
    if (fp == nullptr) {
        throw std::runtime_error("File not open");
    }
    if (not(mode == "w" or mode == "a")) {
        throw std::runtime_error("File not open for writing");
    }
    fseek(fp, 0, SEEK_END);
    fwrite(frame._get_data(), frame.size(), 1, fp);
}



Frame NumpyFile::get_frame(size_t frame_number) {
    Frame frame(m_header.shape[1], m_header.shape[2], m_header.dtype.bitdepth());
    get_frame_into(frame_number, frame._get_data());
    return frame;
}
void NumpyFile::get_frame_into(size_t frame_number, std::byte *image_buf) {
    if (fp == nullptr) {
        throw std::runtime_error("File not open");
    }
    if (frame_number > m_header.shape[0]) {
        throw std::runtime_error("Frame number out of range");
    }
    fseek(fp, header_size + frame_number * bytes_per_frame(), SEEK_SET);
    fread(image_buf, bytes_per_frame(), 1, fp);
}

size_t NumpyFile::pixels() {
    return std::accumulate(m_header.shape.begin() + 1, m_header.shape.end(), 1, std::multiplies<uint64_t>());
};
size_t NumpyFile::bytes_per_frame() { return m_header.dtype.bitdepth() / 8 * pixels(); };

std::vector<Frame> NumpyFile::read(size_t n_frames) {
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
        image_buf += bytes_per_frame();
    }
}

NumpyFile::~NumpyFile() {
    if (mode == "w" or mode == "a") {
        // determine number of frames
        fseek(fp, 0, SEEK_END);
        size_t file_size = ftell(fp);
        size_t data_size = file_size - initial_header_len;
        size_t n_frames = data_size / bytes_per_frame();
        // update number of frames in header (first element of shape)
        m_header.shape[0] = n_frames;
        fseek(fp, 0, SEEK_SET);
        // create string stream to contain header
        std::stringstream ss;
        aare::NumpyHelpers::write_header(ss, m_header);
        std::string header_str = ss.str();
        // write header
        fwrite(header_str.c_str(), header_str.size(), 1, fp);

        
    }

    if (fp != nullptr) {
        fclose(fp);
        
    }
}

void NumpyFile::load_metadata(){

    // read magic number
    std::array<char, 6> tmp{};
    fread(tmp.data(), tmp.size(), 1, fp);
    if (tmp != aare::NumpyHelpers::magic_str) {
        for (auto item : tmp)
            fmt::print("{}, ", int(item));
        fmt::print("\n");
        throw std::runtime_error("Not a numpy file");
    }

    // read version
    fread(reinterpret_cast<char *>(&major_ver_), sizeof(major_ver_), 1,fp);
    fread(reinterpret_cast<char *>(&minor_ver_), sizeof(minor_ver_), 1,fp);

    if (major_ver_ == 1) {
        header_len_size = 2;
    } else if (major_ver_ == 2) {
        header_len_size = 4;
    } else {
        throw std::runtime_error("Unsupported numpy version");
    }

    // read header length
    fread(reinterpret_cast<char *>(&header_len), header_len_size,1, fp);
    header_size = aare::NumpyHelpers::magic_string_length + 2 + header_len_size + header_len;
    if (header_size % 16 != 0) {
        fmt::print("Warning: header length is not a multiple of 16\n");
    }

    // read header
    std::string header(header_len, '\0');
    fread(header.data(), header_len,1,fp);


    // parse header
    std::vector<std::string> keys{"descr", "fortran_order", "shape"};
    aare::logger::debug("original header: \"header\"");

    auto dict_map = aare::NumpyHelpers::parse_dict(header, keys);
    if (dict_map.size() == 0)
        throw std::runtime_error("invalid dictionary in header");

    std::string descr_s = dict_map["descr"];
    std::string fortran_s = dict_map["fortran_order"];
    std::string shape_s = dict_map["shape"];

    std::string descr = aare::NumpyHelpers::parse_str(descr_s);
    aare::DType dtype = aare::NumpyHelpers::parse_descr(descr);

    // convert literal Python bool to C++ bool
    bool fortran_order = aare::NumpyHelpers::parse_bool(fortran_s);

    // parse the shape tuple
    auto shape_v = aare::NumpyHelpers::parse_tuple(shape_s);
    shape_t shape;
    for (auto item : shape_v) {
        auto dim = static_cast<unsigned long>(std::stoul(item));
        shape.push_back(dim);
    }
    m_header = {dtype, fortran_order, shape};
}


} // namespace aare