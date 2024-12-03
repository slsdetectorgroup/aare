#pragma once
#include "aare/FileInterface.hpp"
#include "aare/Frame.hpp"
#include "aare/Hdf5MasterFile.hpp"
#include "aare/NDArray.hpp" //for pixel map

#include <optional>

namespace aare {

/**
 * @brief Class to read .h5 files. The class will parse the master file
 * to find the correct geometry for the frames.
 * @note A more generic interface is available in the aare::File class.
 * Consider using that unless you need hdf5 file specific functionality.
 */
class Hdf5File : public FileInterface {
    Hdf5MasterFile m_master;

    size_t m_current_frame{};
    size_t m_total_frames{};
    size_t m_rows{};
    size_t m_cols{};
    H5::DataType m_datatype{};

    std::unique_ptr<H5::H5File> file{nullptr};
    std::unique_ptr<H5::DataSet> dataset{nullptr};
    std::unique_ptr<H5::DataSpace> dataspace{nullptr};

  public:
    /**
     * @brief Hdf5File constructor
     * @param fname path to the master file (.json)
     * @param mode file mode (only "r" is supported at the moment)

     */
    Hdf5File(const std::filesystem::path &fname, const std::string &mode = "r");
    virtual ~Hdf5File() override;

    Frame read_frame() override;
    Frame read_frame(size_t frame_number) override;
    std::vector<Frame> read_n(size_t n_frames) override;
    void read_into(std::byte *image_buf) override;
    void read_into(std::byte *image_buf, size_t n_frames) override;

    // TODO! do we need to adapt the API?
    void read_into(std::byte *image_buf, DetectorHeader *header);
    void read_into(std::byte *image_buf, size_t n_frames,
                   DetectorHeader *header);

    size_t frame_number(size_t frame_index) override;
    size_t bytes_per_frame() override;
    size_t pixels_per_frame() override;
    size_t bytes_per_pixel() const;
    void seek(size_t frame_index) override;
    size_t tell() override;
    size_t total_frames() const override;
    size_t rows() const override;
    size_t cols() const override;
    size_t bitdepth() const override;
    xy geometry();
    size_t n_mod() const;

    Hdf5MasterFile master() const;

    DetectorType detector_type() const override;

  private:
    /**
     * @brief read the frame at the given frame index into the image buffer
     * @param frame_number frame number to read
     * @param image_buf buffer to store the frame
     */

    void get_frame_into(size_t frame_index, std::byte *frame_buffer,
                        DetectorHeader *header = nullptr);

    /**
     * @brief get the frame at the given frame index
     * @param frame_number frame number to read
     * @return Frame
     */
    Frame get_frame(size_t frame_index);

    /**
     * @brief read the header of the file
     * @param fname path to the data subfile
     * @return DetectorHeader
     */
    static DetectorHeader read_header(const std::filesystem::path &fname);

    static const std::string metadata_group_name;
    void open_file();
};

} // namespace aare