#pragma once
#include "aare/Dtype.hpp"
#include "aare/Frame.hpp"
#include "aare/defs.hpp"

#include <filesystem>
#include <vector>

namespace aare {

/**
 * @brief FileConfig structure to store the configuration of a file
 * dtype: data type of the file
 * rows: number of rows in the file
 * cols: number of columns in the file
 * geometry: geometry of the file
 */
struct FileConfig {
    aare::Dtype dtype{typeid(uint16_t)};
    uint64_t rows{};
    uint64_t cols{};
    bool operator==(const FileConfig &other) const {
        return dtype == other.dtype && rows == other.rows &&
               cols == other.cols && geometry == other.geometry &&
               detector_type == other.detector_type &&
               max_frames_per_file == other.max_frames_per_file;
    }
    bool operator!=(const FileConfig &other) const { return !(*this == other); }

    // rawfile specific
    std::string version{};
    xy geometry{1, 1};
    DetectorType detector_type{DetectorType::Unknown};
    int max_frames_per_file{};
    size_t total_frames{};
    std::string to_string() const {
        return "{ dtype: " + dtype.to_string() +
               ", rows: " + std::to_string(rows) +
               ", cols: " + std::to_string(cols) +
               ", geometry: " + geometry.to_string() +
               ", detector_type: " + ToString(detector_type) +
               ", max_frames_per_file: " + std::to_string(max_frames_per_file) +
               ", total_frames: " + std::to_string(total_frames) + " }";
    }
};

/**
 * @brief FileInterface class to define the interface for file operations
 * @note parent class for NumpyFile and RawFile
 * @note all functions are pure virtual and must be implemented by the derived
 * classes
 */
class FileInterface {
  public:
    /**
     * @brief  one frame from the file at the current position
     * @return Frame
     */
    virtual Frame read_frame() = 0;

    /**
     * @brief read one frame from the file at the given frame number
     * @param frame_number frame number to read
     * @return frame
     */
    virtual Frame read_frame(size_t frame_number) = 0;

    /**
     * @brief read n_frames from the file at the current position
     * @param n_frames number of frames to read
     * @return vector of frames
     */
    virtual std::vector<Frame>
    read_n(size_t n_frames) = 0; // Is this the right interface?

    /**
     * @brief read one frame from the file at the current position and store it
     * in the provided buffer
     * @param image_buf buffer to store the frame
     * @return void
     */
    virtual void read_into(std::byte *image_buf) = 0;

    /**
     * @brief read n_frames from the file at the current position and store them
     * in the provided buffer
     * @param image_buf buffer to store the frames
     * @param n_frames number of frames to read
     * @return void
     */
    virtual void read_into(std::byte *image_buf, size_t n_frames) = 0;

    /**
     * @brief get the frame number at the given frame index
     * @param frame_index index of the frame
     * @return frame number
     */
    virtual size_t frame_number(size_t frame_index) = 0;

    /**
     * @brief get the size of one frame in bytes
     * @return size of one frame
     */
    virtual size_t bytes_per_frame() = 0;

    /**
     * @brief get the number of pixels in one frame
     * @return number of pixels in one frame
     */
    virtual size_t pixels_per_frame() = 0;

    /**
     * @brief seek to the given frame number
     * @param frame_number frame number to seek to
     * @return void
     */
    virtual void seek(size_t frame_number) = 0;

    /**
     * @brief get the current position of the file pointer
     * @return current position of the file pointer
     */
    virtual size_t tell() = 0;

    /**
     * @brief get the total number of frames in the file
     * @return total number of frames in the file
     */
    virtual size_t total_frames() const = 0;
    /**
     * @brief get the number of rows in the file
     * @return number of rows in the file
     */
    virtual size_t rows() const = 0;
    /**
     * @brief get the number of columns in the file
     * @return number of columns in the file
     */
    virtual size_t cols() const = 0;
    /**
     * @brief get the bitdepth of the file
     * @return bitdepth of the file
     */
    virtual size_t bitdepth() const = 0;

    virtual DetectorType detector_type() const = 0;

    // function to query the data type of the file
    /*virtual DataType dtype = 0; */

    virtual ~FileInterface() = default;

  protected:
    std::string m_mode{};
    // std::filesystem::path m_fname{};
    // std::filesystem::path m_base_path{};
    // std::string m_base_name{}, m_ext{};
    // int m_findex{};
    // size_t m_total_frames{};
    // size_t max_frames_per_file{};
    // std::string version{};
    // DetectorType m_type{DetectorType::Unknown};
    // size_t m_rows{};
    // size_t m_cols{};
    // size_t m_bitdepth{};
    // size_t current_frame{};
};

} // namespace aare