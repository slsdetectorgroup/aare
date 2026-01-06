// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/DetectorGeometry.hpp"
#include "aare/FileInterface.hpp"
#include "aare/Frame.hpp"
#include "aare/NDArray.hpp" //for pixel map
#include "aare/ROIGeometry.hpp"
#include "aare/RawMasterFile.hpp"
#include "aare/RawSubFile.hpp"

#ifdef AARE_TESTS
#include "../tests/friend_test.hpp"
#endif

#include <optional>

namespace aare {

/**
 * @brief Class to read .raw files. The class will parse the master file
 * to find the correct geometry for the frames.
 * @note A more generic interface is available in the aare::File class.
 * Consider using that unless you need raw file specific functionality.
 */
class RawFile : public FileInterface {

    std::vector<std::vector<std::unique_ptr<RawSubFile>>>
        m_subfiles; // [ROI][files_per_module]

    RawMasterFile m_master;
    size_t m_current_frame{};

    DetectorGeometry m_geometry;

    /// @brief Geometries e.g. number of modules, size etc. for each ROI
    std::vector<ROIGeometry> m_ROI_geometries;

  public:
    /**
     * @brief RawFile constructor
     * @param fname path to the master file (.json)
     * @param mode file mode (only "r" is supported at the moment)

     */
    RawFile(const std::filesystem::path &fname, const std::string &mode = "r");
    virtual ~RawFile() override = default;

    Frame read_frame() override;
    Frame read_frame(size_t frame_number) override;
    std::vector<Frame> read_n(size_t n_frames) override;

    /**
     * @brief Read all ROIs defined in the master file
     * @param roi_index optional index of the ROI to read (if not provided all
     * ROIs are read)
     * @return vector of Frames (one Frame per ROI)
     */
    std::vector<Frame>
    read_ROIs(const std::optional<size_t> roi_index = std::nullopt);

    /**
     * @brief Read all ROIs defined in the master file for the given frame
     * number
     * @param frame_number frame number to read
     * @param roi_index optional index of the ROI to read (if not provided all
     * ROIs are read)
     * @return vector of Frames (one Frame per ROI)
     */
    std::vector<Frame>
    read_ROIs(const size_t frame_number,
              const std::optional<size_t> roi_index = std::nullopt);

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
    size_t n_modules() const;
    /**
     * @brief number of modules in each ROI
     */
    std::vector<size_t> n_modules_in_roi() const;
    xy geometry() const;

    RawMasterFile master() const;

    DetectorType detector_type() const override;

    /**
     * @brief read the header of the file
     * @param fname path to the data subfile
     * @return DetectorHeader
     */
    static DetectorHeader read_header(const std::filesystem::path &fname);

  private:
    /**
     * @brief read the frame at the given frame index into the image buffer
     * @param frame_index frame number to read
     * @param frame_buffer buffer to store the frame
     * @param roi_index index of the ROI to read (default is 0 e.g. full frame)
     */
    void get_frame_into(size_t frame_index, std::byte *frame_buffer,
                        const size_t roi_index = 0,
                        DetectorHeader *header = nullptr);

    /**
     * @brief get the frame at the given frame index
     * @param frame_number frame number to read
     * @param roi_index index of the ROI to read (default is 0 e.g. full frame)
     * @return Frame
     */
    Frame get_frame(size_t frame_index, const size_t roi_index = 0);

    void open_subfiles(const size_t roi_index);
};

} // namespace aare