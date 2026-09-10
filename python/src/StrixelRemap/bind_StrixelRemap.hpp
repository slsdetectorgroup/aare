
#include <pybind11/pybind11.h>

#include "aare/StrixelPixelRemapping/StrixelPixelRemapDefs.hpp"
#include "aare/StrixelPixelRemapping/StrixelPixelRemapFormat.hpp"

namespace py = pybind11;

void define_PixelStrixelMapDefs(py::module &m) {

    py::enum_<aare::remap::defs::Rotation>(m, "Rotation")
        .value("Identity", aare::remap::defs::Rotation::Identity)
        .value("Rotate180", aare::remap::defs::Rotation::Rotate180)
        .export_values();

    py::enum_<aare::remap::defs::ModuloOrdering>(m, "ModuloOrdering")
        .value("Forward", aare::remap::defs::ModuloOrdering::Forward)
        .value("Reverse", aare::remap::defs::ModuloOrdering::Reverse)
        .export_values();

    py::class_<aare::remap::defs::Guardring>(m, "Guardring")
        .def(py::init<int, int>(), py::arg("x"), py::arg("y"))
        .def_readwrite("x", &aare::remap::defs::Guardring::x,
                       "ring width in pixels")
        .def_readwrite("y", &aare::remap::defs::Guardring::y,
                       "ring height in pixels")
        .def(
            "__eq__",
            [](const aare::remap::defs::Guardring &self,
               const aare::remap::defs::Guardring &other) {
                return self.x == other.x && self.y == other.y;
            },
            py::is_operator())

        .def("__repr__", [](const aare::remap::defs::Guardring &self) {
            return fmt::format("Guardring{{x={}, y={}}}", self.x, self.y);
        });

    py::class_<aare::remap::defs::BondShift>(m, "BondShift")
        .def(py::init<int, int>(), py::arg("x"), py::arg("y"))
        .def_readwrite("x", &aare::remap::defs::BondShift::x,
                       "bond shift in x direction (pixels)")
        .def_readwrite("y", &aare::remap::defs::BondShift::y,
                       "bond shift in y direction (pixels)")
        .def("__repr__", [](const aare::remap::defs::BondShift &self) {
            return fmt::format("BondShift{{x={}, y={}}}", self.x, self.y);
        });

    py::class_<aare::remap::defs::SensorPixelGeometry>(m, "SensorPixelGeometry")
        .def(py::init<int, int, aare::remap::defs::Guardring>(),
             py::arg("num_pix_x"), py::arg("num_pix_y"),
             py::arg("guardring") = aare::remap::defs::Guardring{0, 0})
        .def_readwrite("num_pix_x",
                       &aare::remap::defs::SensorPixelGeometry::num_pix_x,
                       "number of pixels in x direction")
        .def_readwrite("num_pix_y",
                       &aare::remap::defs::SensorPixelGeometry::num_pix_y,
                       "number of pixels in y direction")
        .def_readwrite(
            "guardring", &aare::remap::defs::SensorPixelGeometry::guardring,
            "physical guardring around the sensor (default Guardring(0,0))")

        .def("__repr__",
             [](const aare::remap::defs::SensorPixelGeometry &self) {
                 return fmt::format("SensorPixelGeometry{}",
                                    aare::remap::format::to_string(self));
             });

    py::class_<aare::remap::defs::GroupStrixelGeometry>(m,
                                                        "GroupStrixelGeometry")
        .def(py::init<int, double>(), py::arg("multiplicity"),
             py::arg("pitch_um"))
        .def_readwrite("multiplicity",
                       &aare::remap::defs::GroupStrixelGeometry::multiplicity,
                       "maximum number of pixels a strixel covers")
        .def_readwrite("pitch_um",
                       &aare::remap::defs::GroupStrixelGeometry::pitch_um,
                       "effective minimal strixel pitch [µm]")
        .def("__repr__",
             [](const aare::remap::defs::GroupStrixelGeometry &self) {
                 return fmt::format("GroupStrixelGeometry{}",
                                    aare::remap::format::to_string(self));
             });

    py::class_<aare::remap::defs::GroupRouting>(m, "GroupRouting")
        .def(py::init<aare::remap::defs::ModuloOrdering>(),
             py::arg("mod_order") = aare::remap::defs::ModuloOrdering::Forward)
        .def_readwrite(
            "mod_order", &aare::remap::defs::GroupRouting::mod_order,
            "modulo ordering of pixels within each strixel multiplicity group "
            "default(ModuloOrdering::Forward)")

        .def("__repr__", [](const aare::remap::defs::GroupRouting &self) {
            return fmt::format("GroupRouting{}",
                               aare::remap::format::to_string(self));
        });

    py::class_<aare::remap::defs::GroupConfig>(m, "GroupConfig")
        .def(py::init<aare::remap::defs::GroupStrixelGeometry,
                      aare::remap::defs::GroupRouting, aare::InclusiveROI>(),
             py::arg("strixel"), py::arg("routing"),
             py::arg("placement_on_sensor"))

        .def(py::init([](const aare::remap::defs::GroupStrixelGeometry &strixel,
                         const aare::remap::defs::ModuloOrdering &mod_order,
                         const aare::InclusiveROI &placement_on_sensor) {
                 return aare::remap::defs::GroupConfig{
                     strixel, {mod_order}, placement_on_sensor};
             }),
             py::arg("strixel"), py::arg("routing"),
             py::arg("placement_on_sensor"))
        .def_readwrite("strixel", &aare::remap::defs::GroupConfig::strixel,
                       "strixel geometry of the group")
        .def_readwrite("routing", &aare::remap::defs::GroupConfig::routing,
                       "pixel-to-strixel routing pattern")
        .def_readwrite(
            "placement_on_sensor",
            &aare::remap::defs::GroupConfig::placement_on_sensor,
            "placement of the strixel group on the sensor (sensor-local "
            "coordinates)")

        .def("__repr__", [](const aare::remap::defs::GroupConfig &self) {
            return fmt::format("GroupConfig{}",
                               aare::remap::format::to_string(self));
        });

    py::class_<aare::remap::defs::SensorModulePlacement>(
        m, "SensorModulePlacement")
        .def(py::init<aare::InclusiveROI, aare::remap::defs::Rotation>(),
             py::arg("placement_on_module"), py::arg("rotation"))
        .def_readwrite(
            "placement_on_module",
            &aare::remap::defs::SensorModulePlacement::placement_on_module,
            "sensor bounds in module coordinates")
        .def_readwrite(
            "rotation", &aare::remap::defs::SensorModulePlacement::rotation,
            "physical orientation of the mounted sensor-ASIC assembly with "
            "respect to the module reference frame")

        .def("__repr__",
             [](const aare::remap::defs::SensorModulePlacement &self) {
                 return fmt::format("SensorModulePlacement{}",
                                    aare::remap::format::to_string(self));
             });

    py::class_<aare::remap::defs::StrixelGroupToPixelMap>(
        m, "StrixelGroupToPixelMap")
        .def(py::init<>())
        .def_readonly("effective_roi",
                      &aare::remap::defs::StrixelGroupToPixelMap::effective_roi,
                      "effective pixel ROI covered by this map (InclusiveROI)")

        .def_property_readonly(
            "map",
            [](const aare::remap::defs::StrixelGroupToPixelMap &self)
                -> py::array {
                return py::array_t<ssize_t>(
                    self.map.shape(), self.map.data(),
                    py::cast(&self, py::return_value_policy::reference));
            });
}

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

void define_RemapAlgorithm(py::module &m) {

    m.def("strixel_to_pixel_map", &aare::remap::algo::strixel_to_pixel_map,
          py::arg("group_config").noconvert(), py::arg("pixel").noconvert(),
          py::arg("placement").noconvert(), py::arg("user_roi").noconvert(),
          py::arg("bond_shift").noconvert() =
              aare::remap::defs::BondShift{0, 0},
          R"(
            Creates a StrixeltoPixelMap for a specific GroupConfig

            Parameters
            ----------
            group_config : GroupConfig
                Configuration of the strixel group.
            pixel : SensorPixelGeometry
                Pixel geometry of the sensor.
            placement : SensorModulePlacement
                Placement and orientation of the sensor on the module. 
            user_roi : InclusiveROI
                User-defined region of interest. (in global module coordinates)
            bond_shift : BondShift, optional
                Shift applied to the bond positions. Default is (0, 0).
        
            Returns
            -------
            StrixelGroupToPixelMap
                map(row, col) contains the flattened pixel index of the corresponding source pixel in the user-provided input ROI for strixel defined at (row, col).
                An entry of -1 indicates that the corresponding strixel position has no valid source pixel.

            )");

    // cant use np.take or have to mask -1 indices
    m.def(
        "apply_remap",
        [](py::array input,
           py::array_t<ssize_t, py::array::c_style | py::array::forcecast>
               order_map,
           py::array output) {
            if (!(input.flags() & py::array::c_style) ||
                !(output.flags() & py::array::c_style)) {
                throw std::runtime_error("Arrays must be C-contiguous");
            }

            if (input.ndim() != 2 || output.ndim() != 2) {
                throw std::runtime_error("Input and output arrays must be 2D");
            }

            if (!input.dtype().is(py::dtype::of<uint16_t>())) {
                throw std::runtime_error("Apply remap only supports input "
                                         "arrays of type uint16_t"); // jungfrau
                                                                     // frames
            }

            if (!input.dtype().is(output.dtype())) {
                throw std::runtime_error(
                    "Input and output arrays must have the same dtype");
            }

            auto input_array =
                py::array_t<uint16_t, py::array::c_style |
                                          py::array::forcecast>::ensure(input);

            if (!input_array) {
                throw std::runtime_error("conversion failed");
            }

            auto output_array =
                py::array_t<uint16_t, py::array::c_style |
                                          py::array::forcecast>::ensure(output);

            aare::remap::algo::ApplyRemap(make_view_2d(input_array),
                                          make_view_2d(order_map),
                                          make_view_2d(output_array));
        },
        py::arg("input").noconvert(), py::arg("order_map").noconvert(),
        py::arg("output").noconvert(),
        R"(
            Applies a given remapping rule to an input array.

            Parameters
            ----------
            input : np.array[uint16_t, 2]
                Original array
            order_map : np.array[ssize_t, 2]
                Rule for remapping
            output : np.array[uint16_t, 2]
                Remapped array

            )");
}

template <std::size_t N>
void define_RemapAlgorithmforSensorConfig(py::module &m) {

    m.def("strixel_to_pixel_maps_pybindfunc",
          &aare::remap::algo::strixel_to_pixel_maps<N>,
          py::arg("sensor_config").noconvert(),
          py::arg("placement").noconvert(), py::arg("user_roi").noconvert(),
          py::arg("bond_shift").noconvert() =
              aare::remap::defs::BondShift{0, 0},
          R"(
            Creates a StrixeltoPixelMap for all GroupConfigs in a SensorConfig

            Parameters
            ----------
            sensor_config : SensorConfig
                Configuration of the sensor, including all configurations of the strixel groups.
            placement : SensorModulePlacement
                Placement and orientation of the sensor on the module.
            user_roi : InclusiveROI
                User-defined region of interest. (in global module coordinates)
            bond_shift : BondShift, optional
                Shift applied to the bond positions. Default is (0, 0).
        
            Returns
            -------
            list of StrixelGroupToPixelMap
                map(row, col) contains the flattened pixel index of the corresponding source pixel in the user-provided input ROI for strixel defined at (row, col).
                An entry of -1 indicates that the corresponding strixel position has no valid source pixel.

            )");
}
