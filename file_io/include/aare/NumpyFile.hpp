#pragma once
#include "aare/FileInterface.hpp"
#include "aare/NumpyHelpers.hpp"
#include "aare/defs.hpp"
#include <iostream>
#include <numeric>

class NumpyFile : public FileInterface {
    FILE *fp = nullptr;

  public:
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
    ssize_t bitdepth() const override { return header.dtype.itemsize; }

    NumpyFile(std::filesystem::path fname);
    header_t header{};
    static constexpr std::array<char, 6> magic_str{'\x93', 'N', 'U', 'M', 'P', 'Y'};
    uint8_t major_ver_{};
    uint8_t minor_ver_{};
    uint32_t header_len{};
    uint8_t header_len_size{};
    const uint8_t magic_string_length{6};
    ssize_t header_size{};

    ~NumpyFile() {
        if (fp != nullptr) {
            fclose(fp);
        }
    }

  private:
    size_t current_frame{};
    void get_frame_into(size_t, std::byte *);
    Frame get_frame(size_t frame_number);
};