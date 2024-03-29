#pragma once
#include "aare/FileInterface.hpp"
#include "aare/Frame.hpp"
#include "aare/SubFile.hpp"
#include "aare/defs.hpp"

class RawFile : public FileInterface {

    using config = RawFileConfig;

  public:
    void write(Frame &frame) override{};
    Frame read() override { return get_frame(this->current_frame++); };
    std::vector<Frame> read(size_t n_frames) override;
    void read_into(std::byte *image_buf) override { return get_frame_into(this->current_frame++, image_buf); };
    void read_into(std::byte *image_buf, size_t n_frames) override;
    size_t frame_number(size_t frame_index) override;

    // size of one frame, important fro teh read_into function
    size_t bytes_per_frame() override { return m_rows * m_cols * m_bitdepth / 8; }

    // number of pixels in one frame
    size_t pixels() override { return m_rows * m_cols; }

    // goto frame number
    void seek(size_t frame_number) override{ this->current_frame = frame_number; };

    // return the position of the file pointer (in number of frames)
    size_t tell() override{ return this->current_frame; };

    size_t n_subfiles;
    size_t n_subfile_parts;
    std::vector<std::vector<SubFile *>> subfiles;
    int subfile_rows, subfile_cols;
    xy geometry;
    std::vector<xy> positions;
    config cfg{0, 0};
    TimingMode timing_mode;
    bool quad{false};

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
        return this->m_base_path / fmt::format("{}_master_{}{}", this->m_base_name, this->m_findex, this->m_ext);
    }
    inline std::filesystem::path data_fname(int mod_id, int file_id) {
        return this->m_base_path / fmt::format("{}_d{}_f{}_{}.raw", this->m_base_name, file_id, mod_id, this->m_findex);
    }

    ~RawFile();

    size_t total_frames() const override { return m_total_frames; }
    ssize_t rows() const override { return m_rows; }
    ssize_t cols() const override{ return m_cols; }
    ssize_t bitdepth() const override{ return m_bitdepth; }

  private:
    size_t current_frame{};
    void get_frame_into(size_t frame_number, std::byte *image_buf);
    Frame get_frame(size_t frame_number);
};