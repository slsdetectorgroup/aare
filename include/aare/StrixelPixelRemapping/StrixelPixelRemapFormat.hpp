#pragma once

#include "aare/StrixelPixelRemapping/StrixelPixelRemapDefs.hpp"

namespace aare::remap::format {

std::string to_string(defs::Rotation r);
std::string to_string(defs::ModuloOrdering mo);
std::string to_string(const defs::SensorPixelGeometry &g);
std::string to_string(const defs::GroupStrixelGeometry &g);
std::string to_string(const defs::GroupRouting &r);
std::string to_string(const InclusiveROI &roi);
std::string to_string(const defs::GroupConfig &c);
std::string to_string(const defs::SensorModulePlacement &p);
std::ostream &operator<<(std::ostream &os, const defs::GroupConfig &c);

} // namespace aare::remap::format