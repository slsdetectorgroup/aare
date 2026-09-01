// SPDX-License-Identifier: MPL-2.0
#include "aare/ClusterCollector.hpp"
#include "aare/ClusterFileSink.hpp"
#include "aare/ClusterFinder.hpp"
#include "aare/ClusterFinderMT.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/NDView.hpp"
#include "aare/Pedestal.hpp"
#include "np_helper.hpp"

#include <cstdint>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;
using pd_type = double;

using namespace aare;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

template <typename Type, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = uint16_t>
void define_ClusterVector(py::module &m, const std::string &typestr) {
    using ClusterType = Cluster<Type, ClusterSizeX, ClusterSizeY, CoordType>;
    auto class_name = fmt::format("ClusterVector_{}", typestr);

    py::class_<ClusterVector<
        Cluster<Type, ClusterSizeX, ClusterSizeY, CoordType>, void>>(
        m, class_name.c_str(), R"doc(
        A contiguous, move-only container of fixed-size clusters.

        The class supports the Python buffer protocol, so ``numpy.array`` can
        either copy its data or create a zero-copy view. A zero-copy view is
        valid only while the ClusterVector's underlying allocation and size
        remain unchanged.
        )doc",
        py::buffer_protocol())

        .def(py::init(), R"doc(
        Create an empty ClusterVector with frame number 0 and space reserved
        for at least 1024 clusters.
        )doc")

        .def(
            "__call__",
            [](ClusterVector<ClusterType> &self,
               py::array_t<bool, py::array::c_style> mask) {
                if (mask.ndim() != 1) {
                    throw py::value_error("Mask must be one-dimensional");
                }
                return self(make_view_1d(mask));
            },
            py::arg("mask").noconvert(), R"doc(
            Return a filtered copy of this ClusterVector.

            Parameters
            ----------
            mask : numpy.ndarray
                One-dimensional, writable, C-contiguous array with dtype
                ``numpy.bool_`` and one element per cluster.

            Returns
            -------
            ClusterVector
                Selected clusters in their original order. The frame number is
                preserved.
            )doc")

        .def(
            "push_back",
            [](ClusterVector<ClusterType> &self, const ClusterType &cluster) {
                self.push_back(cluster);
            },
            py::arg("cluster"), R"doc(
            Append one cluster.

            Notes
            -----
            Do not call this method while a zero-copy NumPy view of the
            ClusterVector exists. Reallocation invalidates the view, while an
            append without reallocation leaves its shape unchanged.
            )doc")

        .def(
            "sum",
            [](ClusterVector<ClusterType> &self) {
                auto *vec = new std::vector<Type>(self.sum());
                return return_vector(vec);
            },
            R"doc(
            Return the sum of all pixels in each cluster.

            Returns
            -------
            numpy.ndarray
                One value per cluster, in container order and with the cluster
                pixel dtype.
            )doc")
        .def(
            "sum_2x2",
            [](ClusterVector<ClusterType> &self) {
                auto *vec = new std::vector<Sum_index_pair<Type, corner>>(
                    self.sum_2x2());

                return return_vector(vec);
            },
            R"doc(
            Return the highest-sum center-adjacent 2x2 subcluster for each
            cluster.

            Returns
            -------
            numpy.ndarray
                Structured array with ``sum`` and ``index`` fields. Indices are
                0 for top-left, 1 for top-right, 2 for bottom-left, and 3 for
                bottom-right, relative to the cluster center.
            )doc")
        .def_property_readonly("size", &ClusterVector<ClusterType>::size,
                               "Number of stored clusters.")
        .def("empty", &ClusterVector<ClusterType>::empty,
             "Return True when no clusters are stored.")
        .def("item_size", &ClusterVector<ClusterType>::item_size,
             "Return the size in bytes of one stored cluster, including "
             "padding.")
        .def_property_readonly(
            "fmt",
            [typestr](ClusterVector<ClusterType> &self) {
                return fmt_format<ClusterType>;
            },
            "PEP 3118 format string for one stored cluster.")

        .def_property_readonly("cluster_size_x",
                               &ClusterVector<ClusterType>::cluster_size_x,
                               "Cluster size in the x dimension.")
        .def_property_readonly("cluster_size_y",
                               &ClusterVector<ClusterType>::cluster_size_y,
                               "Cluster size in the y dimension.")
        .def_property_readonly("capacity",
                               &ClusterVector<ClusterType>::capacity,
                               "Number of clusters that fit without "
                               "reallocation.")
        .def_property("frame_number", &ClusterVector<ClusterType>::frame_number,
                      &ClusterVector<ClusterType>::set_frame_number,
                      "Signed 32-bit frame number; 0 can indicate clusters "
                      "from multiple frames.")
        .def_buffer(
            [typestr](ClusterVector<ClusterType> &self) -> py::buffer_info {
                return py::buffer_info(
                    self.data(),             /* Pointer to buffer */
                    self.item_size(),        /* Size of one scalar */
                    fmt_format<ClusterType>, /* Format descriptor */
                    1,                       /* Number of dimensions */
                    {self.size()},           /* Buffer dimensions */
                    {self.item_size()} /* Strides (in bytes) for each index */
                );
            });

    // Free functions using ClusterVector
    m.def(
        "hitmap",
        [](std::array<size_t, 2> image_size, ClusterVector<ClusterType> &cv) {
            // Create a numpy array to hold the hitmap
            // The shape of the array is (image_size[0], image_size[1])
            // note that the python array is passed as [row, col] which
            // is the opposite of the clusters [x,y]
            py::array_t<int32_t> hitmap(image_size);
            auto r = hitmap.mutable_unchecked<2>();

            // Initialize hitmap to 0
            for (py::ssize_t i = 0; i < r.shape(0); i++)
                for (py::ssize_t j = 0; j < r.shape(1); j++)
                    r(i, j) = 0;

            // Loop over the clusters and increment the hitmap
            // Skip out of bound clusters
            for (const auto &cluster : cv) {
                auto x = cluster.x;
                auto y = cluster.y;
                if (x < image_size[1] && y < image_size[0])
                    r(cluster.y, cluster.x) += 1;
            }

            return hitmap;
        },
        py::arg("image_size"), py::arg("clusters"), R"doc(
        Count cluster centers into an ``int32`` image whose shape is given by
        ``image_size`` as ``(rows, columns)``. Element ``[y, x]`` contains the
        number of cluster centers at that coordinate. Out-of-bounds centers are
        ignored.
        )doc");
}

template <typename Type, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = uint16_t>
void define_2x2_reduction(py::module &m) {
    m.def(
        "reduce_to_2x2",
        [](const ClusterVector<
            Cluster<Type, ClusterSizeX, ClusterSizeY, CoordType>> &cv) {
            return new ClusterVector<Cluster<Type, 2, 2, CoordType>>(
                reduce_to_2x2(cv));
        },
        R"doc(
        Reduce every cluster to its highest-sum center-adjacent 2x2 block.
        Returns a new ClusterVector; cluster order, coordinates, and frame
        number are preserved. Input pixel data is interpreted in row-major
        order.
        )doc",
        py::arg("clustervector"));
}

template <typename Type, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = uint16_t>
void define_3x3_reduction(py::module &m) {

    m.def(
        "reduce_to_3x3",
        [](const ClusterVector<
            Cluster<Type, ClusterSizeX, ClusterSizeY, CoordType>> &cv) {
            return new ClusterVector<Cluster<Type, 3, 3, CoordType>>(
                reduce_to_3x3(cv));
        },
        R"doc(
        Reduce every cluster to the 3x3 block around its center index.
        Returns a new ClusterVector; cluster order, coordinates, and frame
        number are preserved.
        )doc",
        py::arg("clustervector"));
}

#pragma GCC diagnostic pop
