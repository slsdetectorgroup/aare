#pragma once
#include "aare/RawMasterFile.hpp" //ROI refactor away
#include "aare/defs.hpp"
namespace aare {

/**
 * @brief Update the detector geometry given a region of interest
 *
 * @param geo
 * @param roi
 * @return DetectorGeometry
 */
DetectorGeometry update_geometry_with_roi(DetectorGeometry geo, ROI roi);

} // namespace aare