#include "aare/ROIGeometry.hpp"
#include <vector>

namespace aare {

ROIGeometry::ROIGeometry(const ROI &roi, DetectorGeometry &geometry)
    : m_pixels_x(roi.width()), m_pixels_y(roi.height()), m_geometry(geometry) {
    m_module_indices_in_roi.reserve(m_geometry.n_modules());
    // determine which modules are in the roi
    for (size_t i = 0; i < m_geometry.n_modules(); ++i) {
        auto module_geometry = m_geometry.get_module_geometries(i);
        if (module_geometry.module_in_roi(roi)) {
            module_geometry.update_geometry_with_roi(roi);
            m_module_indices_in_roi.push_back(i);
        }
    }
};

ROIGeometry::ROIGeometry(DetectorGeometry &geometry)
    : m_pixels_x(geometry.pixels_x()), m_pixels_y(geometry.pixels_y()),
      m_geometry(geometry) {
    m_module_indices_in_roi.reserve(m_geometry.n_modules());
    std::iota(m_module_indices_in_roi.begin(), m_module_indices_in_roi.end(),
              0);
};

size_t ROIGeometry::num_modules_in_roi() const {
    return m_module_indices_in_roi.size();
}

std::vector<size_t> ROIGeometry::module_indices_in_roi() const {
    return m_module_indices_in_roi;
}

size_t ROIGeometry::module_indices_in_roi(const size_t module_index) const {
    return m_module_indices_in_roi.at(module_index);
}

size_t ROIGeometry::pixels_x() const { return m_pixels_x; }
size_t ROIGeometry::pixels_y() const { return m_pixels_y; }

} // namespace aare