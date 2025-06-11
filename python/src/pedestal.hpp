
#include "aare/Pedestal.hpp"
#include "np_helper.hpp"

#include <cstdint>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

template <typename SUM_TYPE>
void define_pedestal_bindings(py::module &m, const std::string &name) {
    py::class_<Pedestal<SUM_TYPE>>(m, name.c_str())
        .def(py::init<int, int, int>())
        .def(py::init<int, int>())
        .def("mean",
             [](Pedestal<SUM_TYPE> &self) {
                 auto mea = new NDArray<SUM_TYPE, 2>{};
                 *mea = self.mean();
                 return return_image_data(mea);
             })
        .def("variance",
             [](Pedestal<SUM_TYPE> &self) {
                 auto var = new NDArray<SUM_TYPE, 2>{};
                 *var = self.variance();
                 return return_image_data(var);
             })
        .def("std",
             [](Pedestal<SUM_TYPE> &self) {
                 auto std = new NDArray<SUM_TYPE, 2>{};
                 *std = self.std();
                 return return_image_data(std);
             })
        .def("clear", py::overload_cast<>(&Pedestal<SUM_TYPE>::clear))
        .def_property_readonly("rows", &Pedestal<SUM_TYPE>::rows)
        .def_property_readonly("cols", &Pedestal<SUM_TYPE>::cols)
        .def_property_readonly("n_samples", &Pedestal<SUM_TYPE>::n_samples)
        .def_property_readonly("sum", &Pedestal<SUM_TYPE>::get_sum)
        .def_property_readonly("sum2", &Pedestal<SUM_TYPE>::get_sum2)
        .def("clone",
             [&](Pedestal<SUM_TYPE> &pedestal) {
                 return Pedestal<SUM_TYPE>(pedestal);
             })
        // TODO! add push for other data types
        .def("push",
             [](Pedestal<SUM_TYPE> &pedestal, py::array_t<uint16_t> &f) {
                 auto v = make_view_2d(f);
                 pedestal.push(v);
             })
        .def(
            "push_no_update",
            [](Pedestal<SUM_TYPE> &pedestal,
               py::array_t<uint16_t, py::array::c_style> &f) {
                auto v = make_view_2d(f);
                pedestal.push_no_update(v);
            },
            py::arg().noconvert())
        .def("update_mean", &Pedestal<SUM_TYPE>::update_mean);
}