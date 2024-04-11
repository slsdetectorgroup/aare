#pragma once
#include "aare/core/defs.hpp"
#include <cstdint>
#include <filesystem>
#include <map>
#include <variant>

namespace aare {

/**
 * @brief Class to read a subfile from a RawFile
 */
class SubFile {
  protected:
    /**
     * @brief type of the read_impl function pointer
     * @param buffer pointer to the buffer to read the data into
     * @return number of bytes read
     */
    using pfunc = size_t (SubFile::*)(std::byte *);
    pfunc read_impl = nullptr;
    /**
     * @brief map to store the read_impl functions for different detectors
     * @note the key is a pair of DetectorType and bitdepth
     * @note the value is a pointer to the read_impl function specific for the detector
     * @note the read_impl function will be set to the appropriate function in the constructor
     */
    std::map<std::pair<DetectorType, int>, pfunc> read_impl_map = {
        {{DetectorType::Moench, 16}, &SubFile::read_impl_reorder<uint16_t>},
        {{DetectorType::Jungfrau, 16}, &SubFile::read_impl_normal},
        {{DetectorType::ChipTestBoard, 16}, &SubFile::read_impl_normal},
        {{DetectorType::Mythen3, 32}, &SubFile::read_impl_normal},
        {{DetectorType::Eiger, 32}, &SubFile::read_impl_normal},
        {{DetectorType::Eiger, 16}, &SubFile::read_impl_normal}

    };

  public:
    /**
     * @brief SubFile constructor
     * @param fname path to the subfile
     * @param detector detector type
     * @param rows number of rows in the subfile
     * @param cols number of columns in the subfile
     * @param bitdepth bitdepth of the subfile
     * @throws std::invalid_argument if the detector,type pair is not supported
     */
    SubFile(std::filesystem::path fname, DetectorType detector, ssize_t rows, ssize_t cols, uint16_t bitdepth);

    /**
     * @brief read the subfile into a buffer
     * @param buffer pointer to the buffer to read the data into
     * @return number of bytes read
     */
    size_t read_impl_normal(std::byte *buffer);

    /**
     * @brief read the subfile into a buffer with the bytes flipped
     * @param buffer pointer to the buffer to read the data into
     * @return number of bytes read
     */
    template <typename DataType> size_t read_impl_flip(std::byte *buffer);

    /**
     * @brief read the subfile into a buffer with the bytes reordered
     * @param buffer pointer to the buffer to read the data into
     * @return number of bytes read
     */
    template <typename DataType> size_t read_impl_reorder(std::byte *buffer);

    /**
     * @brief read the subfile into a buffer with the bytes reordered and flipped
     * @param buffer pointer to the buffer to read the data into
     * @param frame_number frame number to read
     * @return number of bytes read
     */
    size_t get_part(std::byte *buffer, int frame_number);
    size_t frame_number(int frame_index);

    // TODO: define the inlines as variables and assign them in constructor
    inline size_t bytes_per_part() { return (m_bitdepth / 8) * m_rows * m_cols; }
    inline size_t pixels_per_part() { return m_rows * m_cols; }

  protected:
    FILE *fp = nullptr;
    ssize_t m_bitdepth;
    std::filesystem::path m_fname;
    ssize_t m_rows{};
    ssize_t m_cols{};
    ssize_t n_frames{};
    int m_sub_file_index_{};
};

} // namespace aare