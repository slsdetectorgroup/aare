#pragma once

#include "aare/RemapDefs.hpp"

namespace aare::remap::algo {
defs::StrixelGroupToPixelMap
    generate_strixel_to_pixel_map(defs::SensorGroupConfig,
                                  defs::SensorPlacement);
std::vector<defs::StrixelGroupToPixelMap>
    generate_strixel_to_pixel_maps(defs::SensorConfig, defs::SensorPlacement);
} // namespace aare::remap::algo