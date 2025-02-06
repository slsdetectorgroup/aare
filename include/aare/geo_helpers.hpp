#pragma once
#include "aare/defs.hpp"
#include "aare/RawMasterFile.hpp" //ROI refactor away
namespace aare{

/**
 * @brief Update the detector geometry given a region of interest
 * 
 * @param geo 
 * @param roi 
 * @return DetectorGeometry 
 */
DetectorGeometry update_geometry_with_roi(DetectorGeometry geo, ROI roi);


} // namespace aare