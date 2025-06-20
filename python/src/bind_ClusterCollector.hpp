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

template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = uint16_t>
void define_ClusterCollector(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("ClusterCollector_{}", typestr);

    using ClusterType = Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>;

    py::class_<ClusterCollector<ClusterType>>(m, class_name.c_str())
        .def(py::init<ClusterFinderMT<ClusterType, uint16_t, double> *>())
        .def("stop", &ClusterCollector<ClusterType>::stop)
        .def(
            "steal_clusters",
            [](ClusterCollector<ClusterType> &self) {
                auto v = new std::vector<ClusterVector<ClusterType>>(
                    self.steal_clusters());
                return v; // TODO change!!!
            },
            py::return_value_policy::take_ownership);
}

#pragma GCC diagnostic pop