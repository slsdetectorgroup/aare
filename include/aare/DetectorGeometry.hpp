#pragma once
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
 * @brief Class to hold the geometry of a module. Where pixel 0 is located and
 * the size of the module
 */
struct ModuleGeometry {
    int origin_x{};
    int origin_y{};
    int height{};
    int width{};
    int row_index{};
    int col_index{};
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
    void update_geometry_with_roi(ROI roi);

    size_t n_modules() const;

    size_t n_modules_in_roi() const;

    size_t pixels_x() const;
    size_t pixels_y() const;

    size_t modules_x() const;
    size_t modules_y() const;

    const std::vector<ssize_t> &get_modules_in_roi() const;

    ssize_t get_modules_in_roi(const size_t index) const;

    const std::vector<ModuleGeometry> &get_module_geometries() const;

    const ModuleGeometry &get_module_geometries(const size_t index) const;

  private:
    size_t m_modules_x{};
    size_t m_modules_y{};
    size_t m_pixels_x{};
    size_t m_pixels_y{};
    static constexpr ModuleConfig cfg{0, 0};
    std::vector<ModuleGeometry> module_geometries{};
    std::vector<ssize_t> modules_in_roi{};
};

} // namespace aare