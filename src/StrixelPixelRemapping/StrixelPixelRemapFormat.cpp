#include "aare/StrixelPixelRemapping/StrixelPixelRemapFormat.hpp"

#include <sstream>

namespace aare::remap::format {

std::string to_string(defs::Rotation r) {
    return (r == defs::Rotation::Identity ? "Identity" : "Rotate180");
}

std::string to_string(defs::ModuloOrdering mo) {
    return (mo == defs::ModuloOrdering::Forward ? "Forward" : "Reverse");
}

std::string to_string(const defs::SensorPixelGeometry &g) {

    return fmt::format("{{cols x rows: {} x {}, guardring: "
                       "{{x = {}, y = {}}}}}",
                       g.num_pix_x, g.num_pix_y, g.guardring.x, g.guardring.y);
}

std::string to_string(const defs::GroupStrixelGeometry &g) {

    return fmt::format("{{multiplicity: {}, pitch_um: {}}}", g.multiplicity,
                       g.pitch_um);
}

std::string to_string(const defs::GroupRouting &r) {
    return fmt::format("{{{}}}", to_string(r.mod_order));
}

std::string to_string(const InclusiveROI &roi) {
    return fmt::format("{{xmin={}, xmax={}, ymin={}, ymax={}}}", roi.xmin,
                       roi.xmax, roi.ymin, roi.ymax);
}

std::string to_string(const defs::GroupConfig &c) {
    return fmt::format("{{strixel_group: {}, routing: {}, "
                       "placement_on_sensor: {}}}",
                       to_string(c.strixel), to_string(c.routing),
                       to_string(c.placement_on_sensor));
}

std::string to_string(const defs::SensorModulePlacement &p) {
    return fmt::format("{{placement_on_module: {}, "
                       "rotation: {}}}",
                       to_string(p.placement_on_module), to_string(p.rotation));
}

std::ostream &operator<<(std::ostream &os, const defs::GroupConfig &c) {
    return os << to_string(c);
}

} // namespace aare::remap::format
