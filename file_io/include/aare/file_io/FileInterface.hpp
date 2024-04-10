#pragma once
#include "aare/core/DType.hpp"
#include "aare/core/Frame.hpp"
#include "aare/core/defs.hpp"
#include "aare/utils/logger.hpp"
#include <filesystem>
#include <vector>

namespace aare {

struct FileConfig {
    aare::DType dtype = aare::DType(typeid(uint16_t));
    uint64_t rows;
    uint64_t cols;
    xy geometry{1, 1};
    bool operator==(const FileConfig &other) const {
        return dtype == other.dtype && rows == other.rows && cols == other.cols && geometry == other.geometry;
    }
    bool operator!=(const FileConfig &other) const { return !(*this == other); }
};
class FileInterface {
  public:
    friend class FileFactory;
    // write one frame
    virtual void write(Frame &frame) = 0;

    // write n_frames frames
    // virtual void write(std::vector<Frame> &frames) = 0;

    // read one frame
    virtual Frame read() = 0;

    // read n_frames frames
    virtual std::vector<Frame> read(size_t n_frames) = 0; // Is this the right interface?

    // read one frame into the provided buffer
    virtual void read_into(std::byte *image_buf) = 0;

    // read n_frames frame into the provided buffer
    virtual void read_into(std::byte *image_buf, size_t n_frames) = 0;

    // read the frame number on position frame_index
    virtual size_t frame_number(size_t frame_index) = 0;

    // size of one frame, important fro teh read_into function
    virtual size_t bytes_per_frame() = 0;

    // number of pixels in one frame
    virtual size_t pixels() = 0;
    // goto frame number
    virtual void seek(size_t frame_number) = 0;

    // return the position of the file pointer (in number of frames)
    virtual size_t tell() = 0;

    // Getter functions
    virtual size_t total_frames() const = 0;
    virtual size_t rows() const = 0;
    virtual size_t cols() const = 0;
    virtual size_t bitdepth() const = 0;

    // read one frame at position frame_number
    Frame iread(size_t frame_number) {
        auto old_pos = tell();
        seek(frame_number);
        Frame tmp = read();
        seek(old_pos);
        return tmp;
    };

    // read n_frames frames starting at frame_number
    std::vector<Frame> iread(size_t frame_number, size_t n_frames) {
        auto old_pos = tell();
        seek(frame_number);
        std::vector<Frame> tmp = read(n_frames);
        seek(old_pos);
        return tmp;
    }

    // function to query the data type of the file
    /*virtual DataType dtype = 0; */

    virtual ~FileInterface(){

    };

  public:
    std::string m_mode;
    std::filesystem::path m_fname;
    std::filesystem::path m_base_path;
    std::string m_base_name, m_ext;
    int m_findex;
    size_t m_total_frames{};
    size_t max_frames_per_file{};
    std::string version;
    DetectorType m_type;
    size_t m_rows{};
    size_t m_cols{};
    size_t m_bitdepth{};
    size_t current_frame{};

};

} // namespace aare