#include "bind_SensorConfig.hpp"
#include "bind_StrixelRemap.hpp"
#include <pybind11/pybind11.h>

namespace py = pybind11;

#define DEFINE_BINDINGS_SENSORCONFIG(N) define_SensorConfig<N>(m);

#define DEFINE_BINDINGS_STRIXELPIXELMAP(N, M)                                  \
    define_StrixelPixelRemaps<N, M>(m);

void define_StrixelMapsandSensorConfigs(py::module &m) {
    DEFINE_BINDINGS_SENSORCONFIG(1);
    DEFINE_BINDINGS_STRIXELPIXELMAP(1, 1);

    DEFINE_BINDINGS_SENSORCONFIG(2);
    DEFINE_BINDINGS_STRIXELPIXELMAP(2, 2);

    DEFINE_BINDINGS_SENSORCONFIG(4);
    DEFINE_BINDINGS_STRIXELPIXELMAP(4, 4);

    DEFINE_BINDINGS_SENSORCONFIG(3);
    DEFINE_BINDINGS_STRIXELPIXELMAP(3, 3);

    DEFINE_BINDINGS_STRIXELPIXELMAP(2, 1); // quad iLGAD sensor
}
