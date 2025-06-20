#pragma once
#include "aare/Frame.hpp"
#include "aare/defs.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>

namespace aare {

/**
 * @brief Class to read a singe subfile written in .raw format. Used from
 * RawFile to read the entire detector. Can be used directly to read part of the
 * image.
 */
class RawSubFile {
  protected:
    std::ifstream m_file;
    DetectorType m_detector_type;
    size_t m_bitdepth;
    std::filesystem::path m_path; //!< path to the subfile
    std::string m_base_name;      //!< base name used for formatting file names
    size_t m_offset{}; //!< file index of the first file, allow starting at non
                       //!< zero file
    size_t m_total_frames{}; //!< total number of frames in the series of files
    size_t m_rows{};
    size_t m_cols{};
    size_t m_bytes_per_frame{};

    int m_module_index{};
    size_t m_current_file_index{};  //!< The index of the open file
    size_t m_current_frame_index{}; //!< The index of the current frame (with
                                    //!< reference to all files)
    std::vector<size_t>
        m_last_frame_in_file{}; //!< Used for seeking to the correct file

    uint32_t m_pos_row{};
    uint32_t m_pos_col{};

    std::optional<NDArray<ssize_t, 2>> m_pixel_map;

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
    RawSubFile(const std::filesystem::path &fname, DetectorType detector,
               size_t rows, size_t cols, size_t bitdepth, uint32_t pos_row = 0,
               uint32_t pos_col = 0);

    ~RawSubFile() = default;
    /**
     * @brief Seek to the given frame number
     * @note Puts the file pointer at the start of the header, not the start of
     * the data
     * @param frame_index frame position in file to seek to
     * @throws std::runtime_error if the frame number is out of range
     */
    void seek(size_t frame_index);
    size_t tell();

    void read_into(std::byte *image_buf, DetectorHeader *header = nullptr);
    void read_into(std::byte *image_buf, size_t n_frames,
                   DetectorHeader *header = nullptr);
    void get_part(std::byte *buffer, size_t frame_index);

    void read_header(DetectorHeader *header);

    size_t rows() const;
    size_t cols() const;

    size_t frame_number(size_t frame_index);

    size_t bytes_per_frame() const { return m_bytes_per_frame; }
    size_t pixels_per_frame() const { return m_rows * m_cols; }
    size_t bytes_per_pixel() const { return m_bitdepth / bits_per_byte; }

    size_t frames_in_file() const { return m_total_frames; }

  private:
    template <typename T> void read_with_map(std::byte *image_buf);

    void parse_fname(const std::filesystem::path &fname);
    void scan_files();
    void open_file(size_t file_index);
    std::filesystem::path fpath(size_t file_index) const;
};

} // namespace aare