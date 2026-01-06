// SPDX-License-Identifier: MPL-2.0

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
}

size_t DetectorGeometry::n_modules() const { return m_modules_x * m_modules_y; }

size_t DetectorGeometry::pixels_x() const { return m_pixels_x; }
size_t DetectorGeometry::pixels_y() const { return m_pixels_y; }

size_t DetectorGeometry::modules_x() const { return m_modules_x; };
size_t DetectorGeometry::modules_y() const { return m_modules_y; };

const std::vector<ModuleGeometry> &
DetectorGeometry::get_module_geometries() const {
    return module_geometries;
}

const ModuleGeometry &
DetectorGeometry::get_module_geometries(const size_t index) const {
    return module_geometries[index];
}

ModuleGeometry &DetectorGeometry::get_module_geometries(const size_t index) {
    return module_geometries[index];
}

} // namespace aare