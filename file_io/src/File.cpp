#include "aare/File.hpp"
#include "aare/FileFactory.hpp"
#include "aare/utils/logger.hpp"

File::File(std::filesystem::path fname, std::string mode, FileConfig cfg) {
    file_impl = FileFactory::load_file(fname, mode, cfg);
}

void File::write(Frame& frame) { file_impl->write(frame); }
Frame File::read() { return file_impl->read(); }
size_t File::total_frames() const { return file_impl->total_frames(); }
std::vector<Frame> File::read(size_t n_frames) { return file_impl->read(n_frames); }
void File::read_into(std::byte *image_buf) { file_impl->read_into(image_buf); }
void File::read_into(std::byte *image_buf, size_t n_frames) { file_impl->read_into(image_buf, n_frames); }
size_t File::frame_number(size_t frame_index) { return file_impl->frame_number(frame_index); }
size_t File::bytes_per_frame() { return file_impl->bytes_per_frame(); }
size_t File::pixels() { return file_impl->pixels(); }
void File::seek(size_t frame_number) { file_impl->seek(frame_number); }
size_t File::tell() const { return file_impl->tell(); }
ssize_t File::rows() const { return file_impl->rows(); }
ssize_t File::cols() const { return file_impl->cols(); }
ssize_t File::bitdepth() const { return file_impl->bitdepth(); }
File::~File() { delete file_impl; }

Frame File::iread(size_t frame_number) { return file_impl->iread(frame_number); }

File::File(File &&other) {
    file_impl = other.file_impl;
    other.file_impl = nullptr;
}

// write move assignment operator
