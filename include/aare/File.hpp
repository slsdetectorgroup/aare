#pragma once
#include "aare/FileInterface.hpp"
#include <memory>

namespace aare {

/**
 * @brief RAII File class for reading, and in the future potentially writing
 * image files in various formats. Minimal generic interface. For specail
 * fuctions plase use the RawFile or NumpyFile classes directly. Wraps
 * FileInterface to abstract the underlying file format
 * @note **frame_number** refers the the frame number sent by the detector while
 * **frame_index** is the position of the frame in the file
 */
class File {
    std::unique_ptr<FileInterface> file_impl;

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
    File(const std::filesystem::path &fname, const std::string &mode = "r",
         const FileConfig &cfg = {});

    /**Since the object is responsible for managing the file we disable copy
     * construction */
    File(File const &other) = delete;

    /**The same goes for copy assignment */
    File &operator=(File const &other) = delete;

    File(File &&other) noexcept;
    File &operator=(File &&other) noexcept;
    ~File() = default;

    // void close();                             //!< close the file

    Frame
    read_frame(); //!< read one frame from the file at the current position
    Frame read_frame(size_t frame_index); //!< read one frame at the position
                                          //!< given by frame number
    std::vector<Frame> read_n(size_t n_frames); //!< read n_frames from the file
                                                //!< at the current position

    void read_into(std::byte *image_buf);
    void read_into(std::byte *image_buf, size_t n_frames);

    size_t frame_number(); //!< get the frame number at the current position
    size_t frame_number(
        size_t frame_index); //!< get the frame number at the given frame index
    size_t bytes_per_frame() const;
    size_t pixels_per_frame() const;
    size_t bytes_per_pixel() const;
    size_t bitdepth() const;
    void seek(size_t frame_index); //!< seek to the given frame index
    size_t tell() const;           //!< get the frame index of the file pointer
    size_t total_frames() const;
    size_t rows() const;
    size_t cols() const;

    DetectorType detector_type() const;
};

} // namespace aare