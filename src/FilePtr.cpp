
#include "aare/FilePtr.hpp"
#include <fmt/format.h>
#include <stdexcept>
#include <utility>

namespace aare {

FilePtr::FilePtr(const std::filesystem::path &fname,
                 const std::string &mode = "rb") {
    fp_ = fopen(fname.c_str(), mode.c_str());
    if (!fp_)
        throw std::runtime_error(
            fmt::format("Could not open: {}", fname.c_str()));
}

FilePtr::FilePtr(FilePtr &&other) { std::swap(fp_, other.fp_); }

FilePtr &FilePtr::operator=(FilePtr &&other) {
    std::swap(fp_, other.fp_);
    return *this;
}

FILE *FilePtr::get() { return fp_; }

ssize_t FilePtr::tell() {
    auto pos = ftell(fp_);
    if (pos == -1)
        throw std::runtime_error(
            fmt::format("Error getting file position: {}", error_msg()));
    return pos;
}
FilePtr::~FilePtr() {
    if (fp_)
        fclose(fp_); // check?
}

std::string FilePtr::error_msg() {
    if (feof(fp_)) {
        return "End of file reached";
    }
    if (ferror(fp_)) {
        return fmt::format("Error reading file: {}", std::strerror(errno));
    }
    return "";
}
} // namespace aare
