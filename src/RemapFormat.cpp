#include "aare/RemapFormat.hpp"

#include <sstream>

namespace aare::remap::format {
static inline std::string toString(defs::Rotation r) {
    return (r == defs::Rotation::Normal ? "Normal" : "Inverse");
}

static inline std::string toString(defs::SensorPixelGeometry const &g) {
    std::ostringstream os;

    os << "SensorPixelGeometry\n"
       << " cols x rows: " << g.num_pix_x << " x " << g.num_pix_y << "\n"
       << " guardring  :\n"
       << "   x = " << g.guardring.x << "\n"
       << "   y = " << g.guardring.y << "\n";

    return os.str();
}

static inline std::string toString(defs::SensorStrixelGeometry const &g) {
    std::ostringstream os;

    os << "SensorStrixelGeometry\n"
       << " multiplicity: " << g.multiplicity << "\n"
       << " pitch       : " << g.pitch_um << " um\n";

    return os.str();
}

static inline std::string toString(defs::SensorGroupConfig const &c) {
    std::ostringstream os;

    os << "SensorGroupConfig\n"
       << toString(c.pixel) << "\n"
       << toString(c.strixel) << "\n"
       << " placement on sensor:\n"
       << c.placement_on_sensor << "\n";

    return os.str();
}

inline std::ostream &operator<<(std::ostream &os,
                                defs::SensorGroupConfig const &c) {
    return os << toString(c);
}
} // namespace aare::remap::format