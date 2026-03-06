#pragma once

#include "aare/RemapDefs.hpp"

namespace aare::remap::algo {
defs::StrixelGroupToPixelMap
generate_strixel_to_pixel_map(defs::SensorGroupConfig, defs::SensorPlacement,
                              InclusiveROI user_roi);
std::vector<defs::StrixelGroupToPixelMap>
generate_strixel_to_pixel_maps(defs::SensorConfig, defs::SensorPlacement,
                               InclusiveROI user_roi);
} // namespace aare::remap::algo