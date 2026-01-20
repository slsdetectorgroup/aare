#pragma once
#include <cstdio>
#include <filesystem>

namespace aare {

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
    FILE *get();
    ssize_t tell();
    void seek(ssize_t offset, int whence = SEEK_SET) {
        if (fseek(fp_, offset, whence) != 0)
            throw std::runtime_error("Error seeking in file");
    }
    std::string error_msg();
    ~FilePtr();
};

} // namespace aare