// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/VarClusterFinder.hpp"
#include "np_helper.hpp"

#include <cstdint>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <stdexcept>

namespace py = pybind11;
using namespace ::aare;

namespace {

// A numpy view of [first, first + count) of a vector owned by the Python
// object `owner`, which the view keeps alive. No data is copied.
template <typename T>
py::array_t<T> view_vector(std::vector<T> &vec, py::handle owner,
                           size_t first = 0, size_t count = SIZE_MAX) {
    if (count == SIZE_MAX)
        count = vec.size() - first;
    return py::array_t<T>({static_cast<ssize_t>(count)},
                          {static_cast<ssize_t>(sizeof(T))}, vec.data() + first,
                          owner);
}

} // namespace

void define_var_cluster_finder_bindings(py::module &m) {
    using Finder = VarClusterFinder<double>;
    using Hits = Finder::Hits;
    PYBIND11_NUMPY_DTYPE(Finder::Hit, size, row, col, energy, max, offset);

    py::class_<Hits>(m, "VarClusterHits",
                     R"(Clusters returned by VarClusterFinder.steal_hits().

``hits`` is a structured array with one record per cluster: ``size``,
``row`` and ``col`` of the maximum pixel, ``energy``, ``max`` and
``offset``. ``rows``, ``cols`` and ``values`` hold one entry per cluster
pixel; the pixels of ``hits[i]`` are the slice ``offset:offset + size`` of
those arrays, in fill order starting with the seed. All arrays are views
into this object and stay valid as long as any of them is referenced.)")
        .def("__len__", &Hits::size, "Number of clusters.")
        .def_property_readonly(
            "hits",
            [](py::object self) {
                return view_vector(self.cast<Hits &>().hits, self);
            },
            "Structured array with one record per cluster.")
        .def_property_readonly(
            "rows",
            [](py::object self) {
                return view_vector(self.cast<Hits &>().rows, self);
            },
            "Row of every cluster pixel (int16).")
        .def_property_readonly(
            "cols",
            [](py::object self) {
                return view_vector(self.cast<Hits &>().cols, self);
            },
            "Column of every cluster pixel (int16).")
        .def_property_readonly(
            "values",
            [](py::object self) {
                return view_vector(self.cast<Hits &>().values, self);
            },
            "Value of every cluster pixel (float64).")
        .def(
            "pixels",
            [](py::object self, ssize_t i) {
                auto &h = self.cast<Hits &>();
                const ssize_t n = static_cast<ssize_t>(h.size());
                if (i < 0)
                    i += n;
                if (i < 0 || i >= n)
                    throw py::index_error(fmt::format(
                        "hit index {} out of range for {} hits", i, n));
                const auto &hit = h.hits[static_cast<size_t>(i)];
                const auto first = static_cast<size_t>(hit.offset);
                const auto count = static_cast<size_t>(hit.size);
                return py::make_tuple(
                    view_vector(h.rows, self, first, count),
                    view_vector(h.cols, self, first, count),
                    view_vector(h.values, self, first, count));
            },
            py::arg("i"),
            "Rows, columns and values of the pixels of cluster i, as views.");

    py::class_<Finder>(m, "VarClusterFinder",
                       R"(Find clusters of variable size by flood filling
connected pixels above a threshold.

A pixel above the seed threshold starts a cluster that grows into every
4 or 8 connected neighbour above the neighbour threshold. Without a noise
map the thresholds are the fixed ``threshold`` for seeds and
``neighbour_threshold`` for neighbours. With ``noise_map`` set they are
per pixel multiples of the noise, ``seed_factor`` and ``neighbour_factor``.)")
        .def(py::init<Shape<2>, double>(), py::arg("shape"),
             py::arg("threshold"))
        .def_property_readonly(
            "labeled",
            [](const Finder &self) {
                auto *ptr = new NDArray<int32_t, 2>(self.labeled());
                return return_image_data(ptr);
            },
            "Label image of the last frame, as a copy: 0 outside clusters, "
            "the 1-based index of the cluster within that frame on cluster "
            "pixels and -1 on masked pixels.")
        .def_property(
            "noise_map",
            [](const Finder &self) -> py::object {
                const auto &map = self.noise_map();
                if (!map)
                    return py::none();
                return return_image_data(new NDArray<double, 2>(*map));
            },
            [](Finder &self, py::object value) {
                if (value.is_none()) {
                    self.clear_noise_map();
                    return;
                }
                auto map = py::array_t<double,
                                       py::array::c_style |
                                           py::array::forcecast>::ensure(value);
                if (!map)
                    throw py::type_error("noise_map must be an array of the "
                                         "image shape or None");
                self.set_noise_map(make_const_view_2d(map));
            },
            "Per pixel noise of the image shape, or None. When set, the seed "
            "and neighbour thresholds are the threshold factors times the "
            "noise instead of the fixed threshold. The array is copied on "
            "assignment; assign None to return to the fixed threshold.")
        .def_property(
            "mask",
            [](const Finder &self) -> py::object {
                auto mask = self.mask();
                if (!mask)
                    return py::none();
                return return_image_data(
                    new NDArray<bool, 2>(std::move(*mask)));
            },
            [](Finder &self, py::object value) {
                if (value.is_none()) {
                    self.clear_mask();
                    return;
                }
                auto mask =
                    py::array_t<bool, py::array::c_style |
                                          py::array::forcecast>::ensure(value);
                if (!mask)
                    throw py::type_error(
                        "mask must be an array of the image shape or None");
                self.set_mask(make_const_view_2d(mask));
            },
            "Boolean array of the image shape, True on the pixels to "
            "exclude, or None. A masked pixel never seeds a cluster, is never "
            "added to one and is never modified; it is -1 in labeled. The "
            "array is copied on assignment; assign None to remove the mask.")
        .def_property("threshold", &Finder::threshold, &Finder::set_threshold,
                      "Fixed seed threshold, used without a noise map.")
        .def_property(
            "neighbour_threshold",
            [](const Finder &self) { return self.neighbour_threshold(); },
            [](Finder &self, std::optional<double> value) {
                if (value)
                    self.set_neighbour_threshold(*value);
                else
                    self.clear_neighbour_threshold();
            },
            "Fixed neighbour threshold, used without a noise map. Reads as "
            "the value in effect; assign None to follow the seed threshold "
            "again (the default).")
        .def_property("seed_factor", &Finder::seed_factor,
                      &Finder::set_seed_factor,
                      "Seed threshold in units of the noise map (default 5).")
        .def_property(
            "neighbour_factor", &Finder::neighbour_factor,
            &Finder::set_neighbour_factor,
            "Neighbour threshold in units of the noise map (default 5).")
        .def_property(
            "connectivity", &Finder::connectivity, &Finder::set_connectivity,
            "Grow clusters through 4 or 8 connected neighbours (default 8).")
        .def_property(
            "erase_clusters", &Finder::erase_clusters,
            &Finder::set_erase_clusters,
            "Erase the clusters from the input image while processing a "
            "frame (default False): every cluster pixel and the sub-threshold "
            "neighbours around it are set to zero, so that a pedestal updated "
            "from the frame afterwards is not biased by the clusters.")
        .def(
            "find_clusters",
            [](Finder &self, py::array_t<double> img) {
                self.find_clusters(make_view_2d(img));
            },
            py::arg("img").noconvert(),
            "Find the clusters in one float64 frame and append them to the "
            "hits.")
        .def("clear_hits", &Finder::clear_hits, "Discard all accumulated hits.")
        .def("steal_hits", &Finder::steal_hits,
             "Return all accumulated hits as a VarClusterHits and reset the "
             "finder.")
        .def("total_clusters", &Finder::total_clusters,
             "Number of hits currently held by the finder.");
}
