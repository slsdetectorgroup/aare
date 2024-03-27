
#include "aare/NumpyFile.hpp"

NumpyFile::NumpyFile(std::filesystem::path fname_) {
    this->m_fname = fname_;
    fp = fopen(this->m_fname.c_str(), "rb");
}

Frame NumpyFile::get_frame(size_t frame_number) {
    Frame frame(header.shape[1], header.shape[2], header.dtype.itemsize * 8);
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
size_t NumpyFile::bytes_per_frame() { return header.dtype.itemsize * pixels(); };

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

void NumpyFile::write(Frame &frame) {

}
