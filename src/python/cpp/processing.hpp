#include <cstdint>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>

#include "aare/core/Frame.hpp"
#include "aare/core/NDView.hpp"
#include "aare/core/defs.hpp"
#include "aare/file_io/ClusterFileV2.hpp"
#include "aare/file_io/File.hpp"
#include "aare/processing/Pedestal.hpp"

namespace py = pybind11;

template <typename T, typename SUM_TYPE> void define_pedestal_push_bindings(py::class_<Pedestal<SUM_TYPE>> &p) {
    p.def("push", [](Pedestal<SUM_TYPE> &pedestal, py::array_t<T> &np_array) {
        py::buffer_info info = np_array.request();
        if (info.format != py::format_descriptor<T>::format())
            throw std::runtime_error(
                "Incompatible format: different formats! (Are you sure the arrays are of the same type?)");
        if (info.ndim != 2)
            throw std::runtime_error("Incompatible dimension: expected a 2D array!");

        std::array<ssize_t, 2> arr_shape;
        std::move(info.shape.begin(), info.shape.end(), arr_shape.begin());

        NDView<T, 2> a(static_cast<T *>(info.ptr), arr_shape);
        pedestal.push(a);
    });

    p.def("push", [](Pedestal<SUM_TYPE> &pedestal, const int row, const int col, const T val) {
        pedestal.push(row, col, val);
    });
}
template <typename SUM_TYPE> void define_pedestal_bindings(py::module &m) {

    auto p = py::class_<Pedestal<SUM_TYPE>>(m, "Pedestal");
    p.def(py::init<int, int, int>()).def(py::init<int, int>()).def("rows", &Pedestal<SUM_TYPE>::rows);
    // TODO: add DType to Frame so that we can define def_buffer()
    // and we can know what type of values are stored in the frame
    p.def("push", [](Pedestal<SUM_TYPE> &pedestal, Frame &f) {
        if (f.bitdepth() == 8) {
            pedestal.template push<uint8_t>(f);
        } else if (f.bitdepth() == 16) {
            pedestal.template push<uint16_t>(f);
        } else if (f.bitdepth() == 32) {
            pedestal.template push<uint32_t>(f);
        } else if (f.bitdepth() == 64) {
            pedestal.template push<uint64_t>(f);
        } else {
            throw std::runtime_error("Unsupported bitdepth");
        }
    });

    define_pedestal_push_bindings<uint8_t>(p);
    define_pedestal_push_bindings<uint16_t>(p);
    define_pedestal_push_bindings<uint32_t>(p);
    define_pedestal_push_bindings<uint64_t>(p);
    define_pedestal_push_bindings<int8_t>(p);
    define_pedestal_push_bindings<int16_t>(p);
    define_pedestal_push_bindings<int32_t>(p);
    define_pedestal_push_bindings<int64_t>(p);
    define_pedestal_push_bindings<float>(p);
    define_pedestal_push_bindings<double>(p);

    p.def("mean", py::overload_cast<>(&Pedestal<SUM_TYPE>::mean))
        .def("mean", [](Pedestal<SUM_TYPE> &pedestal, const int row, const int col) { return pedestal.mean(row, col); })
        .def("variance", py::overload_cast<>(&Pedestal<SUM_TYPE>::variance))
        .def("variance", [](Pedestal<SUM_TYPE> &pedestal, const int row, const int col) { return pedestal.variance(row, col); })
        .def("standard_deviation", py::overload_cast<>(&Pedestal<SUM_TYPE>::standard_deviation))
        .def("standard_deviation",
             [](Pedestal<SUM_TYPE> &pedestal, const int row, const int col) { return pedestal.standard_deviation(row, col); })
        .def("clear", py::overload_cast<>(&Pedestal<SUM_TYPE>::clear))
        .def("clear", py::overload_cast<const int, const int>(&Pedestal<SUM_TYPE>::clear))
        .def("rows", &Pedestal<SUM_TYPE>::rows)
        .def("cols", &Pedestal<SUM_TYPE>::cols)
        .def("n_samples", &Pedestal<SUM_TYPE>::n_samples)
        .def("index", &Pedestal<SUM_TYPE>::index);

}

void define_processing_bindings(py::module &m) { define_pedestal_bindings<double>(m); }