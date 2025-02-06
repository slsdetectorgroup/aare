#pragma once
#include "aare/defs.hpp"
#include "aare/RawMasterFile.hpp" //ROI refactor away
namespace aare{

DetectorGeometry geometry_from_roi(DetectorGeometry geo, ROI roi);


} // namespace aare