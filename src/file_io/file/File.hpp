#pragma once

#include <filesystem>
#include "defs.hpp"
#include <fmt/core.h>

class File
{
public:
    std::filesystem::path base_path;
    std::string base_name,ext;
    int findex, n_subfiles;
    size_t total_frames{};

    std::string version;
    DetectorType type;
    TimingMode timing_mode;
    int subfile_rows, subfile_cols;




    ssize_t rows{};
    ssize_t cols{};
    uint8_t bitdepth{};
    DetectorType type{};
    // File();
    // ~File();
    

    inline size_t bytes_per_frame() const{
        return rows*cols*bitdepth/8;
        
    }
    inline size_t pixels() const{
        return rows*cols;
    }
    inline std::filesystem::path master_fname() {
    return base_path / fmt::format("{}_master_{}{}", base_name, findex, ext);
    }
    inline std::filesystem::path data_fname(int mod_id, int file_id) {
        return base_path / fmt::format("{}_d{}_f{}_{}.raw", base_name, mod_id, file_id, findex);
    }
    // size_t total_frames();

};
