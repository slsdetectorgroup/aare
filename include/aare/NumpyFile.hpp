#pragma once
#include "aare/Dtype.hpp"
#include "aare/defs.hpp"
#include "aare/FileInterface.hpp"
#include "aare/NumpyHelpers.hpp"


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
    explicit NumpyFile(const std::filesystem::path &fname, const std::string &mode = "r", FileConfig cfg = {});

    void write(Frame &frame);
    Frame read_frame() override { return get_frame(this->current_frame++); }
    Frame read_frame(size_t frame_number) override { return get_frame(frame_number); }

    std::vector<Frame> read_n(size_t n_frames) override;
    void read_into(std::byte *image_buf) override { return get_frame_into(this->current_frame++, image_buf); }
    void read_into(std::byte *image_buf, size_t n_frames) override;
    size_t frame_number(size_t frame_index) override { return frame_index; };
    size_t bytes_per_frame() override;
    size_t pixels_per_frame() override;
    void seek(size_t frame_number) override { this->current_frame = frame_number; }
    size_t tell() override { return this->current_frame; }
    size_t total_frames() const override { return m_header.shape[0]; }
    size_t rows() const override { return m_header.shape[1]; }
    size_t cols() const override { return m_header.shape[2]; }
    size_t bitdepth() const override { return m_header.dtype.bitdepth(); }

    DetectorType detector_type() const override { return DetectorType::Unknown; }

    /**
     * @brief get the data type of the numpy file
     * @return DType
     */
    Dtype dtype() const { return m_header.dtype; }

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
        if (fseek(fp, static_cast<int64_t>(header_size), SEEK_SET)) {
            throw std::runtime_error(LOCATION + "Error seeking to the start of the data");
        }
        size_t rc = fread(arr.data(), sizeof(T), arr.size(), fp);
        if (rc != static_cast<size_t>(arr.size())) {
            throw std::runtime_error(LOCATION + "Error reading data from file");
        }
        return arr;
    }
    template <typename A, typename TYPENAME, A Ndim> void write(NDView<TYPENAME, Ndim> &frame) {
        write_impl(frame.data(), frame.total_bytes());
    }
    template <typename A, typename TYPENAME, A Ndim> void write(NDArray<TYPENAME, Ndim> &frame) {
        write_impl(frame.data(), frame.total_bytes());
    }
    template <typename A, typename TYPENAME, A Ndim> void write(NDView<TYPENAME, Ndim> &&frame) {
        write_impl(frame.data(), frame.total_bytes());
    }
    template <typename A, typename TYPENAME, A Ndim> void write(NDArray<TYPENAME, Ndim> &&frame) {
        write_impl(frame.data(), frame.total_bytes());
    }

    ~NumpyFile() noexcept override;

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
    size_t m_bytes_per_frame{};
    size_t m_pixels_per_frame{};

    size_t m_cols;
    size_t m_rows;
    size_t m_bitdepth;

    void load_metadata();
    void get_frame_into(size_t /*frame_number*/, std::byte * /*image_buf*/);
    Frame get_frame(size_t frame_number);
    void write_impl(void *data, uint64_t size);
};

} // namespace aare