#pragma once
#include "aare/FileInterface.hpp"


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
    File(const std::filesystem::path &fname, const std::string &mode="r", const FileConfig &cfg = {});
      
    Frame read_frame();                       //!< read one frame from the file at the current position
    Frame read_frame(size_t frame_number);    //!< read the frame at the position given by frame number
    std::vector<Frame> read_n(size_t n_frames); //!< read n_frames from the file at the current position

    void read_into(std::byte *image_buf);
    void read_into(std::byte *image_buf, size_t n_frames);
    
    size_t frame_number(size_t frame_index);  //!< get the frame number at the given frame index
    size_t bytes_per_frame();
    size_t pixels_per_frame();
    void seek(size_t frame_number);
    size_t tell() const;
    size_t total_frames() const;
    size_t rows() const;
    size_t cols() const;
    size_t bitdepth() const;
    size_t bytes_per_pixel() const;
    void set_total_frames(size_t total_frames);
    DetectorType detector_type() const;

    File(File &&other) noexcept;
    ~File();
};

} // namespace aare