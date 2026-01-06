// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/ROIGeometry.hpp"
#include "aare/RawMasterFile.hpp" //ROI refactor away
#include "aare/defs.hpp"
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
 * @brief Class to hold the geometry of a module. Where pixel 0 is located
 * relative to full Detector/ROI and the size of the module e.g. size of ROI
 * occupied by the module
 */
struct ModuleGeometry {
    /// @brief start x coordinate of the module in the full detector/ROI
    int origin_x{};
    /// @brief start y coordinate of the module in the full detector/ROI
    int origin_y{};
    /// @brief height of the module in pixels
    int height{};
    /// @brief width of the module in pixels
    int width{};
    /// @brief module index of the module in the detector
    int row_index{};
    /// @brief module index of the module in the detector
    int col_index{};

    bool module_in_roi(const ROI &roi) {
        // module is to the left of the roi
        bool to_the_left = origin_x + width < roi.xmin;
        bool to_the_right = origin_x > roi.xmax;
        bool below = origin_y + height < roi.ymin;
        bool above = origin_y > roi.ymax;

        return !(to_the_left || to_the_right || below || above);
    }

    void update_geometry_with_roi(const ROI &roi) {

        if (!(module_in_roi(roi))) {
            return; // do nothing
        }

        if (roi.xmin >= origin_x && roi.xmin < origin_x + width) {
            width -= roi.xmin - origin_x;
            origin_x = 0;
        } else {
            width = roi.xmax - origin_x < width ? roi.xmax - origin_x : width;
            origin_x -= roi.xmin;
        }

        if (roi.ymin >= origin_y && roi.ymin < origin_y + height) {
            height -= roi.ymin - origin_y;
            origin_y = 0;
        } else {
            height =
                roi.ymax - origin_y < height ? roi.ymax - origin_y : height;
            origin_y -= roi.ymin;
        }
    }
};

/**
 * @brief Class to hold the geometry of a detector. Number of modules, their
 * size and where pixel 0 for each module is located
 */
class DetectorGeometry {
  public:
    DetectorGeometry(const xy &geometry, const ssize_t module_pixels_x,
                     const ssize_t module_pixels_y,
                     const xy udp_interfaces_per_module = xy{1, 1},
                     const bool quad = false);

    ~DetectorGeometry() = default;

    /**
     * @brief Update the detector geometry given a region of interest
     *
     * @param roi
     * @return DetectorGeometry
     */

    size_t n_modules() const;

    size_t pixels_x() const;
    size_t pixels_y() const;

    size_t modules_x() const;
    size_t modules_y() const;

    const std::vector<ModuleGeometry> &get_module_geometries() const;

    const ModuleGeometry &get_module_geometries(const size_t index) const;

    ModuleGeometry &get_module_geometries(const size_t index);

  private:
    size_t m_modules_x{};
    size_t m_modules_y{};
    size_t m_pixels_x{};
    size_t m_pixels_y{};
    static constexpr ModuleConfig cfg{0, 0};

    // TODO: maybe remove - should be a member in ROIGeometry - in particular
    // only works as no overlap between ROIs allowed? maybe just have another
    // additional vector of ModuleGeometry for each ROIGeometry - some
    // duplication but nicer design?
    std::vector<ModuleGeometry> module_geometries;
};

} // namespace aare