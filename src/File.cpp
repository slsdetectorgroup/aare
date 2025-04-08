#include "aare/File.hpp"
#include "aare/JungfrauDataFile.hpp"
#include "aare/NumpyFile.hpp"
#include "aare/RawFile.hpp"

#include <fmt/format.h>

namespace aare {

File::File(const std::filesystem::path &fname, const std::string &mode,
           const FileConfig &cfg)
    : file_impl(nullptr) {
    if (mode != "r") {
        throw std::invalid_argument("At the moment only reading is supported");
    }

    if ((mode == "r") && !std::filesystem::exists(fname)) {
        throw std::runtime_error(
            fmt::format("File does not exist: {}", fname.string()));
    }

    // Assuming we are pointing at a master file? 
    // TODO! How do we read raw files directly?
    if (fname.extension() == ".raw" || fname.extension() == ".json") {
        // file_impl = new RawFile(fname, mode, cfg);
        file_impl = std::make_unique<RawFile>(fname, mode);
    }
    else if (fname.extension() == ".npy") {
        // file_impl = new NumpyFile(fname, mode, cfg);
        file_impl = std::make_unique<NumpyFile>(fname, mode, cfg);
    }else if(fname.extension() == ".dat"){
        file_impl = std::make_unique<JungfrauDataFile>(fname);
    } else {
        throw std::runtime_error("Unsupported file type");
    }
}


File::File(File &&other) noexcept{
    std::swap(file_impl, other.file_impl);
}

File& File::operator=(File &&other) noexcept {
    if (this != &other) {
        File tmp(std::move(other));
        std::swap(file_impl, tmp.file_impl);
    }
    return *this;
}

// void File::close() { file_impl->close(); }

Frame File::read_frame() { return file_impl->read_frame(); }
Frame File::read_frame(size_t frame_index) {
    return file_impl->read_frame(frame_index);
}
size_t File::total_frames() const { return file_impl->total_frames(); }
std::vector<Frame> File::read_n(size_t n_frames) {
    return file_impl->read_n(n_frames);
}

void File::read_into(std::byte *image_buf) { file_impl->read_into(image_buf); }
void File::read_into(std::byte *image_buf, size_t n_frames) {
    file_impl->read_into(image_buf, n_frames);
}

size_t File::frame_number() { return file_impl->frame_number(tell()); }
size_t File::frame_number(size_t frame_index) {
    return file_impl->frame_number(frame_index);
}

size_t File::bytes_per_frame() const { return file_impl->bytes_per_frame(); }
size_t File::pixels_per_frame() const{ return file_impl->pixels_per_frame(); }
void File::seek(size_t frame_index) { file_impl->seek(frame_index); }
size_t File::tell() const { return file_impl->tell(); }
size_t File::rows() const { return file_impl->rows(); }
size_t File::cols() const { return file_impl->cols(); }
size_t File::bitdepth() const { return file_impl->bitdepth(); }
size_t File::bytes_per_pixel() const { return file_impl->bitdepth() / bits_per_byte; }

DetectorType File::detector_type() const { return file_impl->detector_type(); }


} // namespace aare