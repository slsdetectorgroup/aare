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
        m, class_name.c_str(),
        py::buffer_protocol())

        .def(py::init()) // TODO change!!!

        .def("push_back",
             [](ClusterVector<ClusterType> &self, const ClusterType &cluster) {
                 self.push_back(cluster);
             })

        .def("sum",
             [](ClusterVector<ClusterType> &self) {
                 auto *vec = new std::vector<Type>(self.sum());
                 return return_vector(vec);
             })
        .def("sum_2x2",
             [](ClusterVector<ClusterType> &self) {
                 auto *vec = new std::vector<Type>(self.sum_2x2());
                 return return_vector(vec);
             })
        .def_property_readonly("size", &ClusterVector<ClusterType>::size)
        .def("item_size", &ClusterVector<ClusterType>::item_size)
        .def_property_readonly("fmt",
                               [typestr](ClusterVector<ClusterType> &self) {
                                   return fmt_format<ClusterType>;
                               })

        .def_property_readonly("cluster_size_x",
                               &ClusterVector<ClusterType>::cluster_size_x)
        .def_property_readonly("cluster_size_y",
                               &ClusterVector<ClusterType>::cluster_size_y)
        .def_property_readonly("capacity",
                               &ClusterVector<ClusterType>::capacity)
        .def_property("frame_number", &ClusterVector<ClusterType>::frame_number,
                      &ClusterVector<ClusterType>::set_frame_number)
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
    m.def("hitmap",
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
          });
}

#pragma GCC diagnostic pop