#pragma once

#include <filesystem>
#include "defs.hpp"
#include <fmt/core.h>

class File
{
public:
std::filesystem::path fname;
    std::filesystem::path base_path;
    std::string base_name,ext;
    int findex, n_subfiles;
    size_t total_frames{};

    std::string version;
    DetectorType type;
    TimingMode timing_mode;
    int subfile_rows, subfile_cols;
    bool quad {false};




    ssize_t rows{};
    ssize_t cols{};
    uint8_t bitdepth{};
    std::vector<xy> positions;

    // File();
    // ~File();
    



    inline size_t bytes_per_frame() const{
        return rows*cols*bitdepth/8;
    }
    inline size_t pixels() const{
        return rows*cols;
    }

    // TODO! Deal with fast quad and missing files
    inline void find_number_of_subfiles() {
        int n_mod = 0;
        while (std::filesystem::exists(data_fname(n_mod, 0))) {
            n_mod++;
        }
        n_subfiles = n_mod;
    }


    inline std::filesystem::path master_fname() {
    return base_path / fmt::format("{}_master_{}{}", base_name, findex, ext);
    }
    inline std::filesystem::path data_fname(int mod_id, int file_id) {
        return base_path / fmt::format("{}_d{}_f{}_{}.raw", base_name, mod_id, file_id, findex);
    }
    // size_t total_frames();

};
