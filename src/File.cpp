#include "aare/File.hpp"
#include "aare/NumpyFile.hpp"
#include "aare/RawFile.hpp"

#include <fmt/format.h>

namespace aare {

File::File(const std::filesystem::path &fname, const std::string &mode, const FileConfig &cfg)
    : file_impl(nullptr){
    if (mode != "r" && mode != "w" && mode != "a") {
        throw std::invalid_argument("Unsupported file mode");
    }

    if ((mode == "r" || mode == "a") && !std::filesystem::exists(fname)) {
        throw std::runtime_error(fmt::format("File does not exist: {}", fname.string()));
    }

    if (fname.extension() == ".raw" || fname.extension() == ".json") {
        // aare::logger::debug("Loading raw file");
        file_impl = new RawFile(fname, mode, cfg);
    }
    // check if extension is numpy
    else if (fname.extension() == ".npy") {
        // aare::logger::debug("Loading numpy file");
        file_impl = new NumpyFile(fname, mode, cfg);
    } else {
        throw std::runtime_error("Unsupported file type");
    }
}


Frame File::read_frame() { return file_impl->read_frame(); }
size_t File::total_frames() const { return file_impl->total_frames(); }
std::vector<Frame> File::read_n(size_t n_frames) { return file_impl->read_n(n_frames); }
void File::read_into(std::byte *image_buf) { file_impl->read_into(image_buf); }
void File::read_into(std::byte *image_buf, size_t n_frames) { file_impl->read_into(image_buf, n_frames); }
size_t File::frame_number(size_t frame_index) { return file_impl->frame_number(frame_index); }
size_t File::bytes_per_frame() { return file_impl->bytes_per_frame(); }
size_t File::pixels_per_frame() { return file_impl->pixels_per_frame(); }
void File::seek(size_t frame_number) { file_impl->seek(frame_number); }
size_t File::tell() const { return file_impl->tell(); }
size_t File::rows() const { return file_impl->rows(); }
size_t File::cols() const { return file_impl->cols(); }
size_t File::bitdepth() const { return file_impl->bitdepth(); }
size_t File::bytes_per_pixel() const { return file_impl->bitdepth()/8; }
File::~File() { delete file_impl; }
DetectorType File::detector_type() const { return file_impl->detector_type(); }


Frame File::read_frame(size_t frame_number) { return file_impl->read_frame(frame_number); }

File::File(File &&other) noexcept : file_impl(other.file_impl) { other.file_impl = nullptr; }

// write move assignment operator

} // namespace aare