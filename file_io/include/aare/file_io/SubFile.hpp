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
    SubFile(const std::filesystem::path &fname, DetectorType detector, size_t rows, size_t cols, size_t bitdepth,
            int subfile_id);

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
    size_t get_part(std::byte *buffer, size_t frame_number);
    size_t frame_number_in_file(size_t frame_index);
    size_t correct_frame_number(size_t frame_number);
    ~SubFile() noexcept;

    // TODO: define the inlines as variables and assign them in constructor
    inline size_t bytes_per_part() const { return (m_bitdepth / 8) * m_rows * m_cols; }
    inline size_t pixels_per_part() const { return m_rows * m_cols; }

  protected:
    FILE *fp = nullptr;
    size_t m_bitdepth;
    std::filesystem::path m_fname;
    size_t m_rows{};
    size_t m_cols{};
    size_t n_frames{};
    size_t cached_offset{};
    int m_subfile_id{};
};

} // namespace aare