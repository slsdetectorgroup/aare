#pragma once

#include <filesystem>
#include "defs.hpp"

class File
{
private:
    // std::unique_ptr<FileWrapper> fp;
public:
    // File();
    // ~File();
    size_t total_frames{};
    ssize_t rows{};
    ssize_t cols{};
    uint8_t bitdepth{};
    DetectorType type{};

    inline size_t bytes_per_frame() const{
        //TODO: ask if this is correct
        return rows*cols*bitdepth/8;
        
    }
    inline size_t pixels() const{
        return rows*cols;
    }
    // size_t total_frames();

};
