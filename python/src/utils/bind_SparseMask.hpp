#include "aare/utils/SparseMask.hpp"
#include <pybind11/pybind11.h>

namespace py = pybind11;

using namespace aare;

void define_sparse_mask_bindings(py::module &m) {

    py::class_<SparseMask>(m, "SparseMask")
        .def(py::init<STORAGEFORMAT, size_t, size_t>(),
             py::arg("storage_format"), py::arg("rows"), py::arg("cols"))
        .def("insert", &SparseMask::insert, py::arg("row"), py::arg("col"))
        .def("is_masked", &SparseMask::is_masked, py::arg("row"),
             py::arg("col"));
}
