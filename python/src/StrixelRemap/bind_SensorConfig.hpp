#include "aare/StrixelPixelRemapping/StrixelPixelRemapDefs.hpp"
#include <fmt/format.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;

template <std::size_t N> void define_SensorConfig(py::module &m) {
    const auto class_name = fmt::format("SensorConfig_{}PixelGroups", N);
    py::class_<aare::remap::defs::SensorConfig<N>>(m, class_name.c_str())
        .def(py::init<aare::remap::defs::SensorPixelGeometry,
                      std::array<aare::remap::defs::GroupConfig, N>>(),
             py::arg("pixel"), py::arg("group_configs"))
        .def_readwrite("pixel", &aare::remap::defs::SensorConfig<N>::pixel,
                       "sensor pixel geometry (SensorPixelGeometry)")
        .def_readwrite("group_configs",
                       &aare::remap::defs::SensorConfig<N>::group_configs,
                       "[list] of strixel group configurations");
}