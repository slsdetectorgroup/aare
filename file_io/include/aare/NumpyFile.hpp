#pragma once
#include "aare/DType.hpp"
#include "aare/FileInterface.hpp"
#include "aare/NumpyHelpers.hpp"
#include "aare/defs.hpp"
#include <filesystem>
#include <iostream>
#include <numeric>

namespace aare {

class NumpyFile : public FileInterface {
    FILE *fp = nullptr;
    size_t initial_header_len = 0;
    size_t current_frame{};
    std::filesystem::path m_fname;
    uint32_t header_len{};
    uint8_t header_len_size{};
    ssize_t header_size{};
    NumpyHeader m_header;
    uint8_t major_ver_{};
    uint8_t minor_ver_{};

    void load_metadata();
    void get_frame_into(size_t, std::byte *);
    Frame get_frame(size_t frame_number);

  public:
    NumpyFile(const std::filesystem::path &fname);
    NumpyFile(FileConfig, NumpyHeader);
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
    size_t total_frames() const override { return m_header.shape[0]; }
    ssize_t rows() const override { return m_header.shape[1]; }
    ssize_t cols() const override { return m_header.shape[2]; }
    ssize_t bitdepth() const override { return m_header.dtype.bitdepth(); }

    DType dtype() const { return m_header.dtype; }
    std::vector<size_t> shape() const { return m_header.shape; }

    // load the full numpy file into a NDArray
    template <typename T, size_t NDim> NDArray<T, NDim> load() {
        NDArray<T, NDim> arr(make_shape<NDim>(m_header.shape));
        fseek(fp, header_size, SEEK_SET);
        fread(arr.data(), sizeof(T), arr.size(), fp);
        return arr;
    }

    ~NumpyFile();
};

} // namespace aare