
#include <pybind11/pybind11.h>

#include "aare/StrixelPixelRemapping/BaseStrixelPixelMap.hpp"

namespace aare::remap::detail {

struct StrixelPixelMapBindingAccess {
    template <std::size_t N, std::size_t M, typename T>
    static void apply_group_remap(const StrixelPixelMap<N, M> &map,
                                  NDView<T, 2> input, NDView<T, 2> output,
                                  const NDView<const ssize_t, 2> order_map) {
        map.apply_group_remap(input, output, order_map);
    }
};
} // namespace aare::remap::detail

template <std::size_t N, std::size_t M = N, typename T = uint16_t>
void define_StrixelPixelRemaps(py::module &m) {

    const auto class_name =
        fmt::format("StrixelPixelMap_{}Groups_{}Maps", N, M);
    py::class_<aare::remap::StrixelPixelMap<N, M>>(m, class_name.c_str())
        .def(py::init<const aare::remap::defs::SensorConfig<N> &,
                      const aare::remap::defs::SensorModulePlacement &,
                      const aare::remap::defs::BondShift &>(),
             py::arg("sensor_config"), py::arg("module_placement"),
             py::arg("bond_shift") = aare::remap::defs::BondShift{0, 0})

        .def("calculate_map",
             &aare::remap::StrixelPixelMap<N, M>::calculate_map,
             py::arg("user_roi").noconvert(),
             R"(
            Calculate the strixel-to-pixel order maps for all strixel groups
            based on the user-specified ROI.

            Parameters
            ----------
            user_roi : InclusiveROI
                User-specified ROI in the module's native coordinate system.
            )")

        .def_property_readonly(
            "group_maps",
            [](const aare::remap::StrixelPixelMap<N, M> &self) {
                return self.get_group_maps();
            },
            R"(
            Get the strixel-to-pixel order maps for all strixel groups.

            Returns
            -------
            list of StrixelGroupToPixelMap
                List of strixel-to-pixel order maps for each strixel group.
            )")

        .def(
            "__call__",
            [](aare::remap::StrixelPixelMap<N, M> &self,
               const aare::ROI &user_roi,
               py::array_t<T, py::array::c_style | py::array::forcecast>
                   input) {
                if (input.ndim() != 2) {
                    throw std::runtime_error("Input array must be 2D");
                }

                auto mapped_inputs = self(user_roi, make_view_2d(input));

                py::list result_list;
                for (auto mapped_input : mapped_inputs) {
                    auto *mapped_input_ptr =
                        new aare::NDArray<T, 2>(mapped_input);

                    result_list.append(return_image_data(mapped_input_ptr));
                }

                return result_list;
            },
            py::arg("user_roi").noconvert(), py::arg("input").noconvert(),
            R"(
            Apply the strixel-to-pixel remapping to an input array.

            Parameters
            ----------
            user_roi : ROI
                User-specified ROI in the module's native coordinate system.
            input : NDView[uint16_t, 2]
                Input array to be remapped.

            Returns
            -------
            list of NDArray[uint16_t, 2]
                Remapped arrays for each strixel group.
            )")

        .def(
            "__call__",
            [](const aare::remap::StrixelPixelMap<N, M> &self,
               py::array_t<T, py::array::c_style | py::array::forcecast>
                   input) {
                if (input.ndim() != 2) {
                    throw std::runtime_error("Input array must be 2D");
                }

                auto mapped_inputs = self(make_view_2d(input));

                py::list result_list; // TODO: can I reserve space for the list?
                for (auto mapped_input : mapped_inputs) {
                    auto *mapped_input_ptr =
                        new aare::NDArray<T, 2>(mapped_input);

                    result_list.append(return_image_data(mapped_input_ptr));
                }

                return result_list;
            },
            py::arg("input").noconvert(),
            R"(
            Apply the strixel-to-pixel remapping to an input array. 
            This overload assumes that the user ROI has already been set and
            the map calculated using `calculate_map()`.

            Parameters
            ----------
            input : NDView[uint16_t, 2]
                Input array to be remapped.

            Returns
            -------
            list of NDArray[uint16_t, 2]
                Remapped arrays for each strixel group.
            )")

        .def(
            "__call__",
            [](const aare::remap::StrixelPixelMap<N, M> &self,
               py::array_t<T, py::array::c_style | py::array::forcecast> input,
               std::array<
                   py::array_t<T, py::array::c_style | py::array::forcecast>, M>
                   &output) {
                if (input.ndim() != 2) {
                    throw std::runtime_error("Input array must be 2D");
                }

                const auto group_maps = self.get_group_maps();

                const auto input_view = make_view_2d(input);

                for (size_t i = 0; i < group_maps.size(); ++i) {
                    aare::remap::detail::StrixelPixelMapBindingAccess::
                        apply_group_remap(self, input_view,
                                          make_view_2d(output[i]),
                                          group_maps[i].map.view());
                }
            }, // TODO: document empty maps !!!!
            py::arg("input").noconvert(), py::arg("output").noconvert(),
            R"(
            Apply the strixel-to-pixel remapping to an input array.
            This overload assumes that the user ROI has already been set and
            the map calculated using `calculate_map()`.

            Parameters
            ----------
            input : NDView[uint16_t, 2]
                Input array to be remapped.
            output : list of NDArray[uint16_t, 2]
                Preallocated arrays to store the remapped results for each strixel group.
            )");
}
