
#include "aare/Pedestal.hpp"
#include "np_helper.hpp"

#include <cstdint>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

template <typename SUM_TYPE> void define_pedestal_bindings(py::module &m, const std::string &name) {
    py::class_<Pedestal<SUM_TYPE>>(m, name.c_str())
        .def(py::init<int, int, int>())
        .def(py::init<int, int>())
        .def("mean",
             [](Pedestal<SUM_TYPE> &self) {
                 auto m = new NDArray<SUM_TYPE, 2>{};
                 *m = self.mean();
                 return return_image_data(m);
             })
        .def("variance", [](Pedestal<SUM_TYPE> &self) {
            auto m = new NDArray<SUM_TYPE, 2>{};
            *m = self.variance();
            return return_image_data(m);
        })
        .def("std", [](Pedestal<SUM_TYPE> &self) {
            auto m = new NDArray<SUM_TYPE, 2>{};
            *m = self.std();
            return return_image_data(m);
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
        //TODO! add push for other data types
        .def("push", [](Pedestal<SUM_TYPE> &pedestal, py::array_t<uint16_t> &f) {
            auto v = make_view_2d(f);
            pedestal.push(v);
        });
}