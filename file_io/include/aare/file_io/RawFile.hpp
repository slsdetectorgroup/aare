#pragma once
#include "aare/core/Frame.hpp"
#include "aare/file_io/FileInterface.hpp"
#include "aare/file_io/SubFile.hpp"

namespace aare {

/**
 * @brief RawFile class to read .raw and .json files
 * @note derived from FileInterface
 * @note documentation can also be found in the FileInterface class
 */
class RawFile : public FileInterface {
  public:
    /**
     * @brief RawFile constructor
     * @param fname path to the file
     * @param mode file mode (r, w)
     * @param cfg file configuration
     */
    RawFile(const std::filesystem::path &fname, const std::string &mode = "r", const FileConfig &cfg = {});

    /**
     * @brief write function is not implemented for RawFile
     * @param frame frame to write
     */
    void write(Frame &frame) override { throw std::runtime_error("Not implemented"); };
    Frame read() override { return get_frame(this->current_frame++); };
    std::vector<Frame> read(size_t n_frames) override;
    void read_into(std::byte *image_buf) override { return get_frame_into(this->current_frame++, image_buf); };
    void read_into(std::byte *image_buf, size_t n_frames) override;
    size_t frame_number(size_t frame_index) override;

    /**
     * @brief get the number of bytess per frame
     * @return size of one frame in bytes
     */
    size_t bytes_per_frame() override { return m_rows * m_cols * m_bitdepth / 8; }

    /**
     * @brief get the number of pixels in the frame
     * @return number of pixels
     */
    size_t pixels() override { return m_rows * m_cols; }

    // goto frame number
    void seek(size_t frame_number) override { this->current_frame = frame_number; };

    // return the position of the file pointer (in number of frames)
    size_t tell() override { return this->current_frame; };

    /**
     * @brief check if the file is a master file
     * @param fpath path to the file
     */
    static bool is_master_file(std::filesystem::path fpath);

    /**
     * @brief set the module gap row and column
     * @param row gap between rows
     * @param col gap between columns
     */
    inline void set_config(int row, int col) {
        cfg.module_gap_row = row;
        cfg.module_gap_col = col;
    }
    // TODO! Deal with fast quad and missing files

    /**
     * @brief get the number of subfiles for the RawFile
     * @return number of subfiles
     */
    void find_number_of_subfiles();

    /**
     * @brief get the master file name path for the RawFile
     * @return path to the master file
     */
    inline std::filesystem::path master_fname();
    /**
     * @brief get the data file name path for the RawFile with the given module id and file id
     * @param mod_id module id
     * @param file_id file id
     * @return path to the data file
     */
    inline std::filesystem::path data_fname(int mod_id, int file_id);

    /**
     * @brief destructor: will delete the subfiles
     */
    ~RawFile();

    size_t total_frames() const override { return m_total_frames; }
    ssize_t rows() const override { return m_rows; }
    ssize_t cols() const override { return m_cols; }
    ssize_t bitdepth() const override { return m_bitdepth; }

  private:
    /**
     * @brief read the frame at the given frame number into the image buffer
     * @param frame_number frame number to read
     * @param image_buf buffer to store the frame
     */
    void get_frame_into(size_t frame_number, std::byte *image_buf);

    /**
     * @brief get the frame at the given frame number
     * @param frame_number frame number to read
     * @return Frame
     */
    Frame get_frame(size_t frame_number);

    /**
     * @brief parse the file name to get the extension, base name and index
     */
    void parse_fname();

    /**
     * @brief parse the metadata from the file
     */
    void parse_metadata();

    /**
     * @brief parse the metadata of a .raw file
     */
    void parse_raw_metadata();

    /**
     * @brief parse the metadata of a .json file
     */
    void parse_json_metadata();

    /**
     * @brief finds the geometry of the file
     */
    void find_geometry();

    /**
     * @brief read the header of the file
     * @param fname path to the data subfile
     * @return sls_detector_header
     */
    sls_detector_header read_header(const std::filesystem::path &fname);

    /**
     * @brief open the subfiles
     */
    void open_subfiles();

  private:
    size_t n_subfiles;
    size_t n_subfile_parts;
    std::vector<std::vector<SubFile *>> subfiles;
    int subfile_rows, subfile_cols;
    xy geometry;
    std::vector<xy> positions;
    RawFileConfig cfg{0, 0};
    TimingMode timing_mode;
    bool quad{false};
};

} // namespace aare