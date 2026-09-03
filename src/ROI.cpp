#include "aare/ROI.hpp"
#include "aare/DetectorGeometry.hpp"
#include "aare/ROIGeometry.hpp"

namespace aare {

/**
 * @brief Check if the ROI covers the entire detector geometry
 * @param roi Region of interest
 * @param geometry Detector geometry
 * @return true if the ROI covers the entire detector geometry, false otherwise
 */
bool complete_ROI(const ROI &roi, const DetectorGeometry &geometry) {
    return roi.xmin == 0 &&
           roi.xmax == static_cast<ssize_t>(geometry.pixels_x()) &&
           roi.ymin == 0 &&
           roi.ymax == static_cast<ssize_t>(geometry.pixels_y());
}

bool complete_ROI(const std::vector<ROI> &rois,
                  const DetectorGeometry &geometry) {
    if (rois.empty() or rois.size() > 1) {
        return false;
    } else {
        return complete_ROI(rois[0], geometry);
    }
}

bool complete_ROI(const ROIGeometry &roi, const DetectorGeometry &geometry) {
    return roi.pixels_x() == geometry.pixels_x() &&
           roi.pixels_y() == geometry.pixels_y();
}

bool complete_ROI(const std::vector<ROIGeometry> &rois,
                  const DetectorGeometry &geometry) {
    if (rois.empty() or rois.size() > 1) {
        return false;
    } else {
        return complete_ROI(rois[0], geometry);
    }
}

} // namespace aare