
#include "aare/geo_helpers.hpp"
#include "fmt/core.h"

namespace aare{

DetectorGeometry update_geometry_with_roi(DetectorGeometry geo, aare::ROI roi) {
    int pos_y = 0;
    int pos_y_increment = 0;
    for (size_t row = 0; row < geo.modules_y; row++) {
        int pos_x = 0;
        for (size_t col = 0; col < geo.modules_x; col++) {
            auto &m = geo.module_pixel_0[row * geo.modules_x + col];
            auto original_height = m.height;
            auto original_width = m.width;

            // module is to the left of the roi
            if (m.x + m.width < roi.xmin) {
                m.width = 0;

                // roi is in module
            } else {
                // here we only arrive when the roi is in or to the left of
                // the module
                if (roi.xmin > m.x) {
                    m.width -= roi.xmin - m.x;
                }
                if (roi.xmax < m.x + original_width) {
                    m.width -= m.x + original_width - roi.xmax;
                }
                m.x = pos_x;
                pos_x += m.width;
            }

            if (m.y + m.height < roi.ymin) {
                m.height = 0;
            } else {
                if ((roi.ymin > m.y) && (roi.ymin < m.y + m.height)) {
                    m.height -= roi.ymin - m.y;

                }
                if (roi.ymax < m.y + original_height) {
                    m.height -= m.y + original_height - roi.ymax;
                }
                m.y = pos_y;
                pos_y_increment = m.height;
            }
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