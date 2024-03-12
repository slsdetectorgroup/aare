#pragma once
#include "aare/defs.hpp"
#include "aare/Frame.hpp"
#include "aare/File.hpp"


template <DetectorType detector, typename DataType> class JsonFile : public File<detector, DataType> {

        using config = RawFileConfig;
                public:

    Frame<DataType> *get_frame(size_t frame_number);
    int n_subfiles;
    std::vector<SubFile *> subfiles;
    int subfile_rows, subfile_cols;
    std::vector<xy> positions;
    config cfg{0, 0};

    inline void set_config(int row, int col) {
        cfg.module_gap_row = row;
        cfg.module_gap_col = col;
    }
    // TODO! Deal with fast quad and missing files

    void find_number_of_subfiles() {
        int n_mod = 0;
        while (std::filesystem::exists(data_fname(++n_mod, 0)))
            ;
        n_subfiles = n_mod;
    }

    inline std::filesystem::path master_fname() {
        return this->base_path / fmt::format("{}_master_{}{}", this->base_name, this->findex, this->ext);
    }
    inline std::filesystem::path data_fname(int mod_id, int file_id) {
        return this->base_path / fmt::format("{}_d{}_f{}_{}.raw", this->base_name, file_id, mod_id, this->findex);
    }

    ~JsonFile();
};