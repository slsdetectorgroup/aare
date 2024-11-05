#pragma once
#include "aare/Frame.hpp"
#include "aare/defs.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>


namespace aare {

/**
 * @brief Class to read a subfile from a RawFile
 */
class SubFile {
  public:
    size_t write_part(std::byte *buffer, DetectorHeader header, size_t frame_index);
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
            const std::string &mode = "r");

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
    size_t get_part(std::byte *buffer, size_t frame_index);
    size_t frame_number(size_t frame_index);

    // TODO: define the inlines as variables and assign them in constructor
    inline size_t bytes_per_part() const { return (m_bitdepth / 8) * m_rows * m_cols; }
    inline size_t pixels_per_part() const { return m_rows * m_cols; }

    ~SubFile();

  protected:
    FILE *fp = nullptr;
    size_t m_bitdepth;
    std::filesystem::path m_fname;
    size_t m_rows{};
    size_t m_cols{};
    std::string m_mode;
    size_t n_frames{};
    int m_sub_file_index_{};
    DetectorType m_detector_type;
    std::optional<NDArray<size_t, 2>> pixel_map;
};

} // namespace aare