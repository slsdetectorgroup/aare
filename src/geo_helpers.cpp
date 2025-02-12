
#include "aare/geo_helpers.hpp"
#include "fmt/core.h"

namespace aare{

DetectorGeometry update_geometry_with_roi(DetectorGeometry geo, aare::ROI roi) {
    #ifdef AARE_VERBOSE
    fmt::println("update_geometry_with_roi() called with ROI: {} {} {} {}",
                 roi.xmin, roi.xmax, roi.ymin, roi.ymax);
    fmt::println("Geometry: {} {} {} {} {} {}",
                 geo.modules_x, geo.modules_y, geo.pixels_x, geo.pixels_y, geo.module_gap_row, geo.module_gap_col);
    #endif
    int pos_y = 0;
    int pos_y_increment = 0;
    for (int row = 0; row < geo.modules_y; row++) {
        int pos_x = 0;
        for (int col = 0; col < geo.modules_x; col++) {
            auto &m = geo.module_pixel_0[row * geo.modules_x + col];
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
                if ((roi.ymin > m.origin_y) && (roi.ymin < m.origin_y + m.height)) {
                    m.height -= roi.ymin - m.origin_y;

                }
                if (roi.ymax < m.origin_y + original_height) {
                    m.height -= m.origin_y + original_height - roi.ymax;
                }
                m.origin_y = pos_y;
                pos_y_increment = m.height;
            }
            #ifdef AARE_VERBOSE
    fmt::println("Module {} {} {} {}", m.origin_x, m.origin_y, m.width, m.height);
    #endif
        }
        // increment pos_y
        pos_y += pos_y_increment;
    }

    // m_rows = roi.height();
    // m_cols = roi.width();
    geo.pixels_x = roi.width();
    geo.pixels_y = roi.height();

    return geo;

}

} // namespace aare