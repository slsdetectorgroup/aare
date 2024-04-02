#pragma once
#include "aare/FileInterface.hpp"
#include "aare/NumpyHelpers.hpp"
#include "aare/defs.hpp"
#include <iostream>
#include <numeric>
#include <filesystem>

namespace aare{

class NumpyFile : public FileInterface {
    FILE *fp = nullptr;
    size_t initial_header_len = 0;
    


  public:
    std::filesystem::path m_fname; //TO be made private!

    NumpyFile(const std::filesystem::path& fname);
    NumpyFile(FileConfig, header_t);
    void write(Frame &frame) override;
    Frame read() override { return get_frame(this->current_frame++); }

    std::vector<Frame> read(size_t n_frames) override;
    void read_into(std::byte *image_buf) override { return get_frame_into(this->current_frame++, image_buf); }
    void read_into(std::byte *image_buf, size_t n_frames) override;
    size_t frame_number(size_t frame_index) override { return frame_index; };
    size_t bytes_per_frame() override;
    size_t pixels() override;
    void seek(size_t frame_number) override { this->current_frame = frame_number; }
    size_t tell() override { return this->current_frame; }
    size_t total_frames() const override { return header.shape[0]; }
    ssize_t rows() const override { return header.shape[1]; }
    ssize_t cols() const override { return header.shape[2]; }
    ssize_t bitdepth() const override { return header.dtype.bitdepth(); }

    header_t header;
    uint8_t major_ver_{};
    uint8_t minor_ver_{};
    uint32_t header_len{};
    uint8_t header_len_size{};
    ssize_t header_size{};

    ~NumpyFile();

  private:
    size_t current_frame{};
    void get_frame_into(size_t, std::byte *);
    Frame get_frame(size_t frame_number);
};

} // namespace aare