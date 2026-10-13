// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/FileInterface.hpp"
#include "aare/Frame.hpp"
#include "aare/GapPixels.hpp"
#include "aare/NDArray.hpp" //for pixel map
#include "aare/ROIGeometry.hpp"
#include "aare/RawMasterFile.hpp"
#include "aare/RawSubFile.hpp"

#ifdef AARE_TESTS
#include "../tests/friend_test.hpp"
#endif

#include <array>
#include <optional>
#include <random>

namespace aare {

/**
 * @brief Class to read .raw files. The class will parse the master file
 * to find the correct geometry for the frames.
 * @note A more generic interface is available in the aare::File class.
 * Consider using that unless you need raw file specific functionality.
 */
class RawFile : public FileInterface {

    std::vector<std::vector<std::unique_ptr<RawSubFile>>>
        m_subfiles; // [ROI][modules_per_ROI]

    RawMasterFile m_master;
    size_t m_current_frame{};

    /// @brief Minimum frame count across the selected raw subfile series.
    size_t m_frames_in_file{};

    /// @brief Scratch space for one module part that is not contiguous in
    /// the assembled frame. Sized at open to the largest part in any ROI.
    std::vector<std::byte> m_part_buffer;

    /// @brief Gap pixel configuration, empty when reading without gaps
    std::optional<GapPixels> m_gap_pixels;
    detail::GapLayout m_gap_layout{};
    std::mt19937_64 m_generator;

    /// @brief Detector coordinates of each ROI, in roi_geometries() order
    std::vector<ROI> m_roi_rects;
    /// @brief {rows, cols} of the assembled frame of each ROI, including
    /// gap pixels when enabled
    std::vector<std::array<ssize_t, 2>> m_roi_shapes;

  public:
    /**
     * @brief RawFile constructor
     * @param fname path to the master file (.json)
     * @param mode file mode (only "r" is supported at the moment)
     * @param gap_pixels insert gap pixels into every assembled frame. Frame
     * sizes, rows() and cols() then include the gaps. Only supported for
     * Jungfrau and Eiger.
     * @throws std::runtime_error if frame padding is disabled and the frame
     * discard policy is not DiscardPartial.
     * @throws std::invalid_argument if gap pixels are requested for another
     * detector type or the gap configuration is invalid
     */
    RawFile(const std::filesystem::path &fname, const std::string &mode = "r",
            std::optional<GapPixels> gap_pixels = std::nullopt);
    virtual ~RawFile() override = default;

    Frame read_frame() override;
    Frame read_frame(size_t frame_number) override;
    std::vector<Frame> read_n(size_t n_frames) override;

    /**
     * @brief Read one ROI defined in the master file
     * @param roi_index index of the ROI to read
     * @return Frame
     * @note the frame index is incremented after calling this function so
     * reading rois one after the other wont work.
     */
    Frame read_roi(const size_t roi_index);

    /**
     * @brief Read all ROIs defined in the master file
     * @return vector of Frames (one Frame per ROI)
     */
    std::vector<Frame> read_rois();

    /**
     * @brief Read n frames for the given ROI index
     * @param n_frames number of frames to read
     * @param roi_index index of the ROI to read
     * @return vector of Frames
     */
    std::vector<Frame> read_n_with_roi(const size_t n_frames,
                                       const size_t roi_index);

    void read_into(std::byte *image_buf) override;
    void read_into(std::byte *image_buf, size_t n_frames) override;

    // TODO! do we need to adapt the API?
    void read_into(std::byte *image_buf, DetectorHeader *header = nullptr);
    void read_into(std::byte *image_buf, size_t n_frames,
                   DetectorHeader *header);

    void read_roi_into(std::byte *image_buf, const size_t roi_index,
                       const size_t frame_number,
                       DetectorHeader *header =
                           nullptr); // maybe just make get_frame_into public

    size_t frame_number(size_t frame_index) override;
    size_t bytes_per_frame() override;
    // TODO: mmh maybe also pass roi_index in Base class File. Leave it unused
    // for NumpyFile and JungfrauDataFile
    /**
     * @brief bytes per frame for the given ROI
     * @param roi_index index of the ROI
     */
    size_t bytes_per_frame(const size_t roi_index);
    size_t pixels_per_frame() override;
    /**
     * @brief pixels per frame for the given ROI
     * @param roi_index index of the ROI
     */
    size_t pixels_per_frame(const size_t roi_index);
    size_t bytes_per_pixel() const;

    /// @brief Gap pixel configuration, empty when reading without gaps
    const std::optional<GapPixels> &gap_pixels() const { return m_gap_pixels; }

    void seek(size_t frame_index) override;
    size_t tell() override;
    /// @brief Minimum actual frame count across all subfiles and ROIs.
    size_t total_frames() const override;
    size_t rows() const override;
    /**
     * @brief rows for the given ROI
     * @param roi_index index of the ROI
     */
    size_t rows(const size_t roi_index) const;
    size_t cols() const override;
    /**
     * @brief cols for the given ROI
     * @param roi_index index of the ROI
     */
    size_t cols(const size_t roi_index) const;
    size_t bitdepth() const override;
    Dtype dtype() const override { return Dtype::from_bitdepth(bitdepth()); }
    size_t n_modules() const;

    /**
     * @brief number of ROIs defined (always 1 for complete ROI)
     */
    size_t num_rois() const;

    /**
     * @brief get the ROI geometry for the given ROI index
     * @param roi_index index of the ROI
     */
    const ROIGeometry &roi_geometries(size_t roi_index) const;

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
    std::runtime_error frame_error(size_t frame_index,
                                   const std::string &message) const;

    /**
     * @brief Frame index to read from each module part of a ROI so that all
     * parts hold the same detector frame number. Parts whose frame number
     * lags are advanced; the index never moves backwards.
     * @throws std::runtime_error if a part runs out of frames while
     * synchronizing
     */
    std::vector<size_t> synchronized_frame_indices(size_t frame_index,
                                                   size_t roi_index);

    /**
     * @brief Assemble a frame with gap pixels: fill the gap lines, scatter
     * every module part through the scratch buffer and, if configured,
     * split the counts of the chip edge pixels.
     */
    template <typename T>
    void assemble_with_gaps(std::byte *frame_buffer, size_t roi_index,
                            const std::vector<size_t> &frame_indices,
                            DetectorHeader *header);

    /**
     * @brief read the frame at the given frame index into the image buffer
     * @param frame_index frame number to read
     * @param frame_buffer buffer to store the frame
     * @param roi_index index of the ROI to read (default is 0 e.g. full frame)
     */
    void get_frame_into(
        size_t frame_index, std::byte *frame_buffer, const size_t roi_index = 0,
        DetectorHeader *header = nullptr); // TODO read_into updates it!!!!

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
