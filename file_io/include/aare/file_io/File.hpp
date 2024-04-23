#pragma once
#include "aare/file_io/FileInterface.hpp"

namespace aare {

/**
 * @brief RAII File class for reading and writing image files in various formats
 * wrapper on a FileInterface to abstract the underlying file format
 * @note documentation for each function is in the FileInterface class
 */
class File {
  private:
    FileInterface *file_impl;
    bool is_npy = true;

  public:
    /**
     * @brief Construct a new File object
     * @param fname path to the file
     * @param mode file mode (r, w, a)
     * @param cfg file configuration
     * @throws std::runtime_error if the file cannot be opened
     * @throws std::invalid_argument if the file mode is not supported
     *
     */
    File(const std::filesystem::path &fname, const std::string &mode, const FileConfig &cfg = {});
    void write(Frame &frame, sls_detector_header header = {});
    Frame read();
    Frame iread(size_t frame_number);
    std::vector<Frame> read(size_t n_frames);
    void read_into(std::byte *image_buf);
    void read_into(std::byte *image_buf, size_t n_frames);
    size_t frame_number(size_t frame_index);
    size_t bytes_per_frame();
    size_t pixels_per_frame();
    void seek(size_t frame_number);
    size_t tell() const;
    size_t total_frames() const;
    size_t rows() const;
    size_t cols() const;
    size_t bitdepth() const;

    /**
     * @brief Move constructor
     * @param other File object to move from
     */
    File(File &&other) noexcept;

    /**
     * @brief destructor: will only delete the FileInterface object
     */
    ~File();
};

} // namespace aare