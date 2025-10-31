#include "aare/CalculateEta.hpp"

#include <cstdint>
#include <pybind11/pybind11.h>

namespace py = pybind11;
using namespace ::aare;

template <typename Type, uint8_t CoordSizeX, uint8_t CoordSizeY,
          typename CoordType = uint16_t>
void register_calculate_2x2eta(py::module &m) {
    using ClusterType = Cluster<Type, CoordSizeX, CoordSizeY, CoordType>;

    m.def(
        "calculate_eta2",
        [](const aare::ClusterVector<ClusterType> &clusters) {
            auto eta2 = new NDArray<double, 2>(calculate_eta2(clusters));
            return return_image_data(eta2);
        },
        R"(calculates eta2x2)", py::arg("clusters"));

    m.def(
        "calculate_eta2",
        [](const aare::Cluster<Type, CoordSizeX, CoordSizeY, CoordType>
               &cluster) {
            auto eta2 = calculate_eta2(cluster);
            // TODO return proper eta class
            return py::make_tuple(eta2.x, eta2.y, eta2.sum);
        },
        R"(calculates eta2x2)", py::arg("cluster"));

    m.def(
        "calculate_full_eta2",
        [](const aare::Cluster<Type, CoordSizeX, CoordSizeY, CoordType>
               &cluster) {
            auto eta2 = calculate_full_eta2(cluster);
            return py::make_tuple(eta2.x, eta2.y, eta2.sum);
        },
        R"(calculates full eta2x2)", py::arg("cluster"));

    m.def(
        "calculate_full_eta2",
        [](const aare::ClusterVector<ClusterType> &clusters) {
            auto eta2 = new NDArray<double, 2>(calculate_full_eta2(clusters));
            return return_image_data(eta2);
        },
        R"(calculates full eta2x2)", py::arg("clusters"));
}

template <typename Type, typename CoordType = uint16_t>
void register_calculate_3x3eta(py::module &m) {
    using ClusterType = Cluster<Type, 3, 3, CoordType>;

    m.def(
        "calculate_eta3",
        [](const aare::ClusterVector<ClusterType> &clusters) {
            auto eta = new NDArray<double, 2>(calculate_eta3(clusters));
            return return_image_data(eta);
        },
        R"(calculates eta3x3 using entire cluster)", py::arg("clusters"));

    m.def(
        "calculate_cross_eta3",
        [](const aare::ClusterVector<ClusterType> &clusters) {
            auto eta = new NDArray<double, 2>(calculate_cross_eta3(clusters));
            return return_image_data(eta);
        },
        R"(calculates eta3x3 taking into account cross pixels in cluster)",
        py::arg("clusters"));

    m.def(
        "calculate_eta3",
        [](const ClusterType &cluster) {
            auto eta = calculate_eta3(cluster);
            // TODO return proper eta class
            return py::make_tuple(eta.x, eta.y, eta.sum);
        },
        R"(calculates eta3x3 using entire cluster)", py::arg("cluster"));

    m.def(
        "calculate_cross_eta3",
        [](const ClusterType &cluster) {
            auto eta = calculate_cross_eta3(cluster);
            // TODO return proper eta class
            return py::make_tuple(eta.x, eta.y, eta.sum);
        },
        R"(calculates eta3x3 taking into account cross pixels in cluster)",
        py::arg("cluster"));
}