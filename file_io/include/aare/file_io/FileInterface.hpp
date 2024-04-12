#pragma once
#include "aare/core/DType.hpp"
#include "aare/core/Frame.hpp"
#include "aare/core/defs.hpp"
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
    aare::DType dtype = aare::DType(typeid(uint16_t));
    uint64_t rows{};
    uint64_t cols{};
    xy geometry{1, 1};
    bool operator==(const FileConfig &other) const {
        return dtype == other.dtype && rows == other.rows && cols == other.cols && geometry == other.geometry;
    }
    bool operator!=(const FileConfig &other) const { return !(*this == other); }
};

/**
 * @brief FileInterface class to define the interface for file operations
 * @note parent class for NumpyFile and RawFile
 * @note all functions are pure virtual and must be implemented by the derived classes
 */
class FileInterface {
  public:
    /**
     * @brief write a frame to the file
     * @param frame frame to write
     * @return void
     * @throws std::runtime_error if the function is not implemented
     */
    virtual void write(Frame &frame) = 0;

    /**
     * @brief write a vector of frames to the file
     * @param frames vector of frames to write
     * @return void
     */
    // virtual void write(std::vector<Frame> &frames) = 0;

    /**
     * @brief read one frame from the file at the current position
     * @return Frame
     */
    virtual Frame read() = 0;

    /**
     * @brief read n_frames from the file at the current position
     * @param n_frames number of frames to read
     * @return vector of frames
     */
    virtual std::vector<Frame> read(size_t n_frames) = 0; // Is this the right interface?

    /**
     * @brief read one frame from the file at the current position and store it in the provided buffer
     * @param image_buf buffer to store the frame
     * @return void
     */
    virtual void read_into(std::byte *image_buf) = 0;

    /**
     * @brief read n_frames from the file at the current position and store them in the provided buffer
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

    /**
     * @brief read one frame from the file at the given frame number
     * @param frame_number frame number to read
     * @return frame
     */
    Frame iread(size_t frame_number) {
        auto old_pos = tell();
        seek(frame_number);
        Frame tmp = read();
        seek(old_pos);
        return tmp;
    };

    /**
     * @brief read n_frames from the file starting at the given frame number
     * @param frame_number frame number to start reading from
     * @param n_frames number of frames to read
     * @return vector of frames
     */
    std::vector<Frame> iread(size_t frame_number, size_t n_frames) {
        auto old_pos = tell();
        seek(frame_number);
        std::vector<Frame> tmp = read(n_frames);
        seek(old_pos);
        return tmp;
    }

    // function to query the data type of the file
    /*virtual DataType dtype = 0; */

    virtual ~FileInterface() = default;

  protected:
    std::string m_mode{};
    std::filesystem::path m_fname{};
    std::filesystem::path m_base_path{};
    std::string m_base_name{}, m_ext{};
    int m_findex{};
    size_t m_total_frames{};
    size_t max_frames_per_file{};
    std::string version{};
    DetectorType m_type{};
    size_t m_rows{};
    size_t m_cols{};
    size_t m_bitdepth{};
    size_t current_frame{};
};

} // namespace aare