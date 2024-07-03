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
#include "aare/processing/ClusterFinder.hpp"
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

        std::array<int64_t, 2> arr_shape;
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
    // TODO: add DType to Frame so that we can define def_buffer()
    // and we can know what type of values are stored in the frame
    p.def(py::init<int, int, int>())
        .def(py::init<int, int>())
        .def("set_freeze", &Pedestal<SUM_TYPE>::set_freeze)
        .def("mean", py::overload_cast<>(&Pedestal<SUM_TYPE>::mean))
        .def("mean", [](Pedestal<SUM_TYPE> &pedestal, const int row, const int col) { return pedestal.mean(row, col); })
        .def("variance", py::overload_cast<>(&Pedestal<SUM_TYPE>::variance))
        .def("variance",
             [](Pedestal<SUM_TYPE> &pedestal, const int row, const int col) { return pedestal.variance(row, col); })
        .def("standard_deviation", py::overload_cast<>(&Pedestal<SUM_TYPE>::standard_deviation))
        .def("standard_deviation", [](Pedestal<SUM_TYPE> &pedestal, const int row,
                                      const int col) { return pedestal.standard_deviation(row, col); })
        .def("clear", py::overload_cast<>(&Pedestal<SUM_TYPE>::clear))
        .def("clear", py::overload_cast<const int, const int>(&Pedestal<SUM_TYPE>::clear))
        .def_property_readonly("rows", &Pedestal<SUM_TYPE>::rows)
        .def_property_readonly("cols", &Pedestal<SUM_TYPE>::cols)
        .def_property_readonly("n_samples", &Pedestal<SUM_TYPE>::n_samples)
        .def_property_readonly("index", &Pedestal<SUM_TYPE>::index)
        .def_property_readonly("sum", &Pedestal<SUM_TYPE>::get_sum)
        .def_property_readonly("sum2", &Pedestal<SUM_TYPE>::get_sum2);
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
}

template <typename VIEW_TYPE, typename PEDESTAL_TYPE = double>
void define_cluster_finder_template_bindings(py::class_<ClusterFinder> &cf) {
    cf.def("find_clusters_without_threshold",
           py::overload_cast<NDView<VIEW_TYPE, 2>, Pedestal<PEDESTAL_TYPE> &, bool>(
               &ClusterFinder::find_clusters_without_threshold<VIEW_TYPE, PEDESTAL_TYPE>));
    cf.def("find_clusters_with_threshold", py::overload_cast<NDView<VIEW_TYPE, 2>, Pedestal<PEDESTAL_TYPE> &>(
                                               &ClusterFinder::find_clusters_with_threshold<VIEW_TYPE, PEDESTAL_TYPE>));

    cf.def("find_clusters_without_threshold", [](ClusterFinder &self, py::array_t<VIEW_TYPE> &np_array,
                                                 Pedestal<PEDESTAL_TYPE> &pedestal, bool late_update) {
        py::buffer_info info = np_array.request();
        if (info.format != Dtype(typeid(VIEW_TYPE)).numpy_descr())
            throw std::runtime_error(
                "Incompatible format: different formats! (Are you sure the arrays are of the same type?)");
        if (info.ndim != 2)
            throw std::runtime_error("Incompatible dimension: expected a 2D array!");

        std::array<int64_t, 2> arr_shape;
        std::copy(info.shape.begin(), info.shape.end(), arr_shape.begin());

        NDView<VIEW_TYPE, 2> a(static_cast<VIEW_TYPE *>(info.ptr), arr_shape);
        return self.find_clusters_without_threshold(a, pedestal, late_update);
    });

    cf.def("find_clusters_with_threshold",
           [](ClusterFinder &self, py::array_t<VIEW_TYPE> &np_array, Pedestal<PEDESTAL_TYPE> &pedestal) {
               py::buffer_info info = np_array.request();
               if (info.format != py::format_descriptor<VIEW_TYPE>::format())
                   throw std::runtime_error(
                       "Incompatible format: different formats! (Are you sure the arrays are of the same type?)");
               if (info.ndim != 2)
                   throw std::runtime_error("Incompatible dimension: expected a 2D array!");

               std::array<int64_t, 2> arr_shape;
               std::copy(info.shape.begin(), info.shape.end(), arr_shape.begin());

               NDView<VIEW_TYPE, 2> a(static_cast<VIEW_TYPE *>(info.ptr), arr_shape);
               return self.find_clusters_with_threshold(a, pedestal);
           });
}
void define_cluster_finder_bindings(py::module &m) {

    py::class_<ClusterFinder> cf(m, "ClusterFinder");
    cf.def(py::init<int, int, double, double>());
    define_cluster_finder_template_bindings<uint8_t>(cf);
    define_cluster_finder_template_bindings<uint16_t>(cf);
    define_cluster_finder_template_bindings<uint32_t>(cf);
    define_cluster_finder_template_bindings<uint64_t>(cf);
    define_cluster_finder_template_bindings<int8_t>(cf);
    define_cluster_finder_template_bindings<int16_t>(cf);
    define_cluster_finder_template_bindings<int32_t>(cf);
    define_cluster_finder_template_bindings<int64_t>(cf);
    define_cluster_finder_template_bindings<float>(cf);
    define_cluster_finder_template_bindings<double>(cf);
}
void define_processing_bindings(py::module &m) {
    define_pedestal_bindings<double>(m);

    py::class_<Cluster>(m, "Cluster",py::buffer_protocol())
        .def(py::init<int, int, Dtype>())
        .def("size", &Cluster::size)
        .def("begin", &Cluster::begin)
        .def("end", &Cluster::end)
        .def_readwrite("x", &Cluster::x)
        .def_readwrite("y", &Cluster::y)
        .def_buffer([](Cluster &c) -> py::buffer_info {
            return py::buffer_info(c.data(), c.dt.bytes(), c.dt.format_descr(), 1, {c.size()}, {c.dt.bytes()});
        })

        .def("__repr__", [](const Cluster &a) {
            return "<Cluster: x: " + std::to_string(a.x) + ", y: " + std::to_string(a.y) + ">";
        });
    define_cluster_finder_bindings(m);
}