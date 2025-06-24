
#include "aare/DetectorGeometry.hpp"
#include "fmt/core.h"
#include <iostream>
#include <vector>

namespace aare {

DetectorGeometry::DetectorGeometry(const xy &geometry,
                                   const ssize_t module_pixels_x,
                                   const ssize_t module_pixels_y,
                                   const xy udp_interfaces_per_module,
                                   const bool quad) {

    size_t num_modules = geometry.col * geometry.row;
    module_geometries.reserve(num_modules);
    for (size_t col = 0; col < geometry.col;
         col += udp_interfaces_per_module.col)
        for (size_t row = 0; row < geometry.row;
             row += udp_interfaces_per_module.row) {
            for (size_t port_row = 0; port_row < udp_interfaces_per_module.row;
                 ++port_row)
                for (size_t port_col = 0;
                     port_col < udp_interfaces_per_module.col; ++port_col) {
                    ModuleGeometry g;
                    g.row_index =
                        quad ? (row + port_row + 1) % 2 : (row + port_row);
                    g.col_index = col + port_col;
                    g.origin_x = g.col_index * module_pixels_x;
                    g.origin_y = g.row_index * module_pixels_y;

                    g.width = module_pixels_x;
                    g.height = module_pixels_y;
                    module_geometries.push_back(g);
                }
        }

    m_pixels_y = (geometry.row * module_pixels_y);
    m_pixels_x = (geometry.col * module_pixels_x);
    m_modules_x = geometry.col;
    m_modules_y = geometry.row;
    m_pixels_y += static_cast<size_t>((geometry.row - 1) * cfg.module_gap_row);

    modules_in_roi.resize(num_modules);
    std::iota(modules_in_roi.begin(), modules_in_roi.end(), 0);
}

size_t DetectorGeometry::n_modules() const { return m_modules_x * m_modules_y; }

size_t DetectorGeometry::n_modules_in_roi() const {
    return modules_in_roi.size();
};

size_t DetectorGeometry::pixels_x() const { return m_pixels_x; }
size_t DetectorGeometry::pixels_y() const { return m_pixels_y; }

size_t DetectorGeometry::modules_x() const { return m_modules_x; };
size_t DetectorGeometry::modules_y() const { return m_modules_y; };

const std::vector<ssize_t> &DetectorGeometry::get_modules_in_roi() const {
    return modules_in_roi;
}

ssize_t DetectorGeometry::get_modules_in_roi(const size_t index) const {
    return modules_in_roi[index];
}

const std::vector<ModuleGeometry> &
DetectorGeometry::get_module_geometries() const {
    return module_geometries;
}

const ModuleGeometry &
DetectorGeometry::get_module_geometries(const size_t index) const {
    return module_geometries[index];
}

void DetectorGeometry::update_geometry_with_roi(ROI roi) {
#ifdef AARE_VERBOSE
    fmt::println("update_geometry_with_roi() called with ROI: {} {} {} {}",
                 roi.xmin, roi.xmax, roi.ymin, roi.ymax);
    fmt::println("Geometry: {} {} {} {} {} {}", m_modules_x, m_modules_y,
                 m_pixels_x, m_pixels_y, cfg.module_gap_row,
                 cfg.module_gap_col);

#endif

    modules_in_roi.clear();
    modules_in_roi.reserve(m_modules_x * m_modules_y);
    int pos_y = 0;
    int pos_y_increment = 0;
    for (size_t row = 0; row < m_modules_y; row++) {
        int pos_x = 0;
        for (size_t col = 0; col < m_modules_x; col++) {
            auto &m = module_geometries[row * m_modules_x + col];

            auto original_height = m.height;
            auto original_width = m.width;

            // module is to the left of the roi
            if (m.origin_x + m.width < roi.xmin) {
                m.width = 0;

                // roi is in module
            } else {
                // here we only arrive when the roi is in or to the left of
                // the module
                if (roi.xmin > m.origin_x) {
                    m.width -= roi.xmin - m.origin_x;
                }
                if (roi.xmax < m.origin_x + original_width) {
                    m.width -= m.origin_x + original_width - roi.xmax;
                }
                m.origin_x = pos_x;
                pos_x += m.width;
            }

            if (m.origin_y + m.height < roi.ymin) {
                m.height = 0;
            } else {
                if ((roi.ymin > m.origin_y) &&
                    (roi.ymin < m.origin_y + m.height)) {
                    m.height -= roi.ymin - m.origin_y;
                }
                if (roi.ymax < m.origin_y + original_height) {
                    m.height -= m.origin_y + original_height - roi.ymax;
                }
                m.origin_y = pos_y;
                pos_y_increment = m.height;
            }

            if (m.height != 0 && m.width != 0) {
                modules_in_roi.push_back(row * m_modules_x + col);
            }

#ifdef AARE_VERBOSE
            fmt::println("Module {} {} {} {}", m.origin_x, m.origin_y, m.width,
                         m.height);
#endif
        }
        // increment pos_y
        pos_y += pos_y_increment;
    }

    // m_rows = roi.height();
    // m_cols = roi.width();
    m_pixels_x = roi.width();
    m_pixels_y = roi.height();
}

} // namespace aare