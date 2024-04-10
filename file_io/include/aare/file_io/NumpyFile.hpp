#pragma once
#include "aare/core/DType.hpp"
#include "aare/core/defs.hpp"
#include "aare/file_io/FileInterface.hpp"
#include "aare/file_io/NumpyHelpers.hpp"
#include <filesystem>
#include <iostream>
#include <numeric>

namespace aare {

/**
 * @brief NumpyFile class to read and write numpy files
 * @note derived from FileInterface
 * @note implements all the pure virtual functions from FileInterface
 * @note documentation for the functions can also be found in the FileInterface class
 */
class NumpyFile : public FileInterface {

  public:
    /**
     * @brief NumpyFile constructor
     * @param fname path to the numpy file
     * @param mode file mode (r, w)
     * @param cfg file configuration
     */
    NumpyFile(const std::filesystem::path &fname, const std::string &mode = "r", FileConfig cfg = {});

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

    /**
     * @brief get the data type of the numpy file
     * @return DType
     */
    DType dtype() const { return m_header.dtype; }

    /**
     * @brief get the shape of the numpy file
     * @return vector of type size_t
     */
    std::vector<size_t> shape() const { return m_header.shape; }

    /**
     * @brief load the numpy file into an NDArray
     * @tparam T data type of the NDArray
     * @tparam NDim number of dimensions of the NDArray
     * @return NDArray<T, NDim>
     */
    template <typename T, size_t NDim> NDArray<T, NDim> load() {
        NDArray<T, NDim> arr(make_shape<NDim>(m_header.shape));
        fseek(fp, header_size, SEEK_SET);
        fread(arr.data(), sizeof(T), arr.size(), fp);
        return arr;
    }

    ~NumpyFile();

  private:
    FILE *fp = nullptr;
    size_t initial_header_len = 0;
    size_t current_frame{};
    uint32_t header_len{};
    uint8_t header_len_size{};
    size_t header_size{};
    NumpyHeader m_header;
    uint8_t major_ver_{};
    uint8_t minor_ver_{};

    void load_metadata();
    void get_frame_into(size_t, std::byte *);
    Frame get_frame(size_t frame_number);
};

} // namespace aare