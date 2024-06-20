#pragma once
#include "aare/core/Frame.hpp"
#include "aare/file_io/FileInterface.hpp"
#include "aare/file_io/SubFile.hpp"

namespace aare {

struct ModuleConfig {
    int module_gap_row{};
    int module_gap_col{};

    bool operator==(const ModuleConfig &other) const {
        if (module_gap_col != other.module_gap_col)
            return false;
        if (module_gap_row != other.module_gap_row)
            return false;
        return true;
    }
};

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
    explicit RawFile(const std::filesystem::path &fname, const std::string &mode = "r",
                     const FileConfig &config = FileConfig{});

    /**
     * @brief write function is not implemented for RawFile
     * @param frame frame to write
     */
    void write(Frame &frame, sls_detector_header header);
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
    size_t pixels_per_frame() override { return m_rows * m_cols; }

    // goto frame index
    void seek(size_t frame_index) override {
        // check if the frame number is greater than the total frames
        // if frame_number == total_frames, then the next read will throw an error
        if (frame_index > this->total_frames()) {
            throw std::runtime_error(
                fmt::format("frame number {} is greater than total frames {}", frame_index, m_total_frames));
        }
        this->current_frame = frame_index;
    };

    // return the position of the file pointer (in number of frames)
    size_t tell() override { return this->current_frame; };

    /**
     * @brief check if the file is a master file
     * @param fpath path to the file
     */
    static bool is_master_file(const std::filesystem::path &fpath);

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
    inline std::filesystem::path data_fname(size_t mod_id, size_t file_id);

    /**
     * @brief destructor: will delete the subfiles
     */
    ~RawFile() noexcept override;

    size_t total_frames() const override { return m_total_frames; }
    size_t rows() const override { return m_rows; }
    size_t cols() const override { return m_cols; }
    size_t bitdepth() const override { return m_bitdepth; }
    xy geometry() { return m_geometry; }

  private:
    void write_master_file();
    /**
     * @brief read the frame at the given frame index into the image buffer
     * @param frame_number frame number to read
     * @param image_buf buffer to store the frame
     */
    void get_frame_into(size_t frame_index, std::byte *frame_buffer);

    /**
     * @brief get the frame at the given frame index
     * @param frame_number frame number to read
     * @return Frame
     */
    Frame get_frame(size_t frame_index);

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
    static sls_detector_header read_header(const std::filesystem::path &fname);

    /**
     * @brief open the subfiles
     */
    void open_subfiles();
    void parse_config(const FileConfig &config);

    size_t n_subfiles{};
    size_t n_subfile_parts{};
    std::vector<std::vector<SubFile *>> subfiles;
    size_t subfile_rows{}, subfile_cols{};
    xy m_geometry{};
    std::vector<xy> positions;
    ModuleConfig cfg{0, 0};
    TimingMode timing_mode{};
    bool quad{false};
};

} // namespace aare