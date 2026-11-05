// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/defs.hpp"
#include <cstdint>
#include <cstdio>
#include <filesystem>

namespace aare {

// MSVC's long is 32 bits, so fseek/ftell stop working past 2 GB there
inline int fseek64(FILE *fp, int64_t offset, int whence) {
#ifdef _WIN32
    return _fseeki64(fp, offset, whence);
#else
    return fseek(fp, static_cast<long>(offset), whence);
#endif
}

inline int64_t ftell64(FILE *fp) {
#ifdef _WIN32
    return _ftelli64(fp);
#else
    return ftell(fp);
#endif
}

/**
 * \brief RAII wrapper for FILE pointer
 */
class FilePtr {
    FILE *fp_{nullptr};

  public:
    FilePtr() = default;
    FilePtr(const std::filesystem::path &fname, const std::string &mode);
    FilePtr(const FilePtr &) = delete;            // we don't want a copy
    FilePtr &operator=(const FilePtr &) = delete; // since we handle a resource
    FilePtr(FilePtr &&other);
    FilePtr &operator=(FilePtr &&other);
    explicit operator bool() const noexcept;
    FILE *get();
    ssize_t tell();
    void seek(ssize_t offset, int whence = SEEK_SET) {
        if (fseek64(fp_, offset, whence) != 0)
            throw std::runtime_error("Error seeking in file");
    }
    std::string error_msg();
    ~FilePtr();
};

} // namespace aare
