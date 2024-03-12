#pragma once

#include "aare/defs.hpp"
#include "aare/Frame.hpp"
#include "SubFile.hpp"
#include <filesystem>
#include <fmt/core.h>
#include <iostream>


template <DetectorType detector, typename DataType>
class File {

  public:
    virtual Frame<DataType>* get_frame(int frame_number) = 0;

  private:
  //comment
  

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
    uint8_t bitdepth{};
    // File();
    

    inline size_t bytes_per_frame() const { return rows * cols * bitdepth / 8; }
    inline size_t pixels() const { return rows * cols; }


    // size_t total_frames();
};