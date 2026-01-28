#pragma once
#include "aare/DetectorGeometry.hpp"
#include "aare/defs.hpp"

namespace aare {

class DetectorGeometry; // forward declaration to avoid circular dependency

/**
 * @brief Class to hold the geometry of a region of interest (ROI)
 */
class ROIGeometry {
  public:
    /** @brief Constructor
     * @param roi
     * @param geometry general detector geometry
     */
    ROIGeometry(const ROI &roi, DetectorGeometry &geometry);

    /** @brief Constructor for ROI geometry expanding over full detector
     * @param geometry general detector geometry
     */
    ROIGeometry(DetectorGeometry &geometry);

    /// @brief Get number of modules in the ROI
    size_t num_modules_in_roi() const;

    /// @brief Get the indices of modules in the ROI
    const std::vector<size_t> &module_indices_in_roi() const;

    size_t module_indices_in_roi(const size_t module_index) const;

    size_t pixels_x() const;
    size_t pixels_y() const;

  private:
    /// @brief Size of the ROI in pixels in x direction
    size_t m_pixels_x{};
    /// @brief Size of the ROI in pixels in y direction
    size_t m_pixels_y{};

    DetectorGeometry &m_geometry;

    /// @brief Indices of modules included in the ROI
    std::vector<size_t> m_module_indices_in_roi;
};

} // namespace aare