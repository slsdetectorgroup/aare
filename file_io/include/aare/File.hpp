#pragma once

#include "SubFile.hpp"
#include "aare/Frame.hpp"
#include "aare/defs.hpp"
#include <filesystem>
#include <fmt/core.h>
#include <iostream>

class File {

  public:
    virtual Frame get_frame(size_t frame_number) = 0;

  private:
    // comment

  public:
    virtual ~File() = default; 
    std::filesystem::path fname;
    std::filesystem::path base_path;
    std::string base_name, ext;
    int findex;
    size_t total_frames{};
    size_t max_frames_per_file{};

    std::string version;
    DetectorType type;
    TimingMode timing_mode;
    bool quad{false};

    ssize_t rows{};
    ssize_t cols{};
    ssize_t bitdepth{};
    // File();

    inline size_t bytes_per_frame() const { return rows * cols * bitdepth / 8; }
    inline size_t pixels() const { return rows * cols; }

    // size_t total_frames();
};