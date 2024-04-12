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
    File(std::filesystem::path fname, std::string mode, FileConfig cfg = {});
    void write(Frame &frame);
    Frame read();
    Frame iread(size_t frame_number);
    std::vector<Frame> read(size_t n_frames);
    void read_into(std::byte *image_buf);
    void read_into(std::byte *image_buf, size_t n_frames);
    size_t frame_number(size_t frame_index);
    size_t bytes_per_frame();
    size_t pixels();
    void seek(size_t frame_number);
    size_t tell() const;
    size_t total_frames() const;
    ssize_t rows() const;
    ssize_t cols() const;
    ssize_t bitdepth() const;

    /**
     * @brief Move constructor
     * @param other File object to move from
     */
    File(File &&other);

    /**
     * @brief destructor: will only delete the FileInterface object
     */
    ~File();
};

} // namespace aare