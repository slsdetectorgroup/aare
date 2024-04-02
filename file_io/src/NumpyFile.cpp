
#include "aare/NumpyFile.hpp"

namespace aare{

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

NumpyFile::NumpyFile(std::filesystem::path fname_) {
    this->m_fname = fname_;
    fp = fopen(this->m_fname.c_str(), "rb");
}
NumpyFile::NumpyFile(FileConfig config, header_t header) {
    this->mode = "w";
    this->m_fname = config.fname;
    this->m_bitdepth = config.dtype.bitdepth();
    this->m_rows = config.rows;
    this->m_cols = config.cols;
    this->header = header;
    this->header.shape = {0, config.rows, config.cols};

    fp = fopen(this->m_fname.c_str(), "wb");
    if (!fp) {
        throw std::runtime_error(fmt::format("Could not open: {} for reading", this->m_fname.c_str()));
    }

    this->initial_header_len =
        aare::NumpyHelpers::write_header(std::filesystem::path(this->m_fname.c_str()), this->header);
}

Frame NumpyFile::get_frame(size_t frame_number) {
    Frame frame(header.shape[1], header.shape[2], header.dtype.bitdepth());
    get_frame_into(frame_number, frame._get_data());
    return frame;
}
void NumpyFile::get_frame_into(size_t frame_number, std::byte *image_buf) {
    if (fp == nullptr) {
        throw std::runtime_error("File not open");
    }
    if (frame_number > header.shape[0]) {
        throw std::runtime_error("Frame number out of range");
    }
    fseek(fp, header_size + frame_number * bytes_per_frame(), SEEK_SET);
    fread(image_buf, bytes_per_frame(), 1, fp);
}

size_t NumpyFile::pixels() {
    return std::accumulate(header.shape.begin() + 1, header.shape.end(), 1, std::multiplies<uint64_t>());
};
size_t NumpyFile::bytes_per_frame() { return header.dtype.bitdepth() / 8 * pixels(); };

std::vector<Frame> NumpyFile::read(size_t n_frames) {
    // TODO: implement this in a more efficient way
    std::vector<Frame> frames;
    for (size_t i = 0; i < n_frames; i++) {
        frames.push_back(this->get_frame(this->current_frame));
        this->current_frame++;
    }
    return frames;
}
void NumpyFile::read_into(std::byte *image_buf, size_t n_frames) {
    // TODO: implement this in a more efficient way
    for (size_t i = 0; i < n_frames; i++) {
        this->get_frame_into(this->current_frame++, image_buf);
        image_buf += this->bytes_per_frame();
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
        this->header.shape[0] = n_frames;
        fseek(fp, 0, SEEK_SET);
        // create string stream to contain header
        std::stringstream ss;
        aare::NumpyHelpers::write_header(ss, this->header);
        std::string header_str = ss.str();
        // write header
        fwrite(header_str.c_str(), header_str.size(), 1, fp);

        
    }

    if (fp != nullptr) {
        fclose(fp);
        
    }
}

} // namespace aare