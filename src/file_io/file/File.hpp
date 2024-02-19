#pragma once

#include "defs.hpp"
#include <filesystem>
#include <fmt/core.h>
#include "SubFile.hpp"

struct RawFileConfig {
    int module_gap_row{};
    int module_gap_col{};

    bool operator==(const RawFileConfig &other) const {
        if (module_gap_col != other.module_gap_col)
            return false;
        if (module_gap_row != other.module_gap_row)
            return false;
        return true;
    }
};



class File {
    private:
    using config = RawFileConfig;
  public:
    std::vector<SubFileVariants> subfiles;
    std::filesystem::path fname;
    std::filesystem::path base_path;
    std::string base_name, ext;
    int findex, n_subfiles;
    size_t total_frames{};

    std::string version;
    DetectorType type;
    TimingMode timing_mode;
    int subfile_rows, subfile_cols;
    bool quad{false};

    ssize_t rows{};
    ssize_t cols{};
    uint8_t bitdepth{};
    std::vector<xy> positions;

    config cfg{0,0};
    // File();
    // ~File();

    inline size_t bytes_per_frame() const { return rows * cols * bitdepth / 8; }
    inline size_t pixels() const { return rows * cols; }
    inline void set_config(int row,int col){
        cfg.module_gap_row = row;
        cfg.module_gap_col = col;
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
        return base_path /
               fmt::format("{}_master_{}{}", base_name, findex, ext);
    }
    inline std::filesystem::path data_fname(int mod_id, int file_id) {
        return base_path / fmt::format("{}_d{}_f{}_{}.raw", base_name, mod_id,
                                       file_id, findex);
    }
    // size_t total_frames();
};
