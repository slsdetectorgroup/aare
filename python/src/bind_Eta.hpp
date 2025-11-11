#include "aare/CalculateEta.hpp"

#include <cstdint>
// #include <pybind11/native_enum.h> only for version 3
#include <pybind11/pybind11.h>

namespace py = pybind11;
using namespace ::aare;

template <typename T>
void define_eta(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("Eta{}", typestr);

    py::class_<Eta2<T>>(m, class_name.c_str())
        .def(py::init<>())
        .def_readonly("x", &Eta2<T>::x, "eta x value")
        .def_readonly("y", &Eta2<T>::y, "eta y value")
        .def_readonly("c", &Eta2<T>::c,
                      "eta corner value cTopLeft, cTopRight, "
                      "cBottomLeft, cBottomRight")
        .def_readonly("sum", &Eta2<T>::sum, "photon energy of cluster");
}

void define_corner_enum(py::module &m) {
    py::enum_<corner>(m, "corner", "enum.Enum")
        .value("cTopLeft", corner::cTopLeft)
        .value("cTopRight", corner::cTopRight)
        .value("cBottomLeft", corner::cBottomLeft)
        .value("cBottomRight", corner::cBottomRight)
        .export_values();
}

template <typename Type, uint8_t CoordSizeX, uint8_t CoordSizeY,
          typename CoordType = uint16_t>
void register_calculate_2x2eta(py::module &m) {
    using ClusterType = Cluster<Type, CoordSizeX, CoordSizeY, CoordType>;

    m.def(
        "calculate_eta2",
        [](const aare::ClusterVector<ClusterType> &clusters) {
            auto eta2 = new std::vector<Eta2<typename ClusterType::value_type>>(
                calculate_eta2(clusters));
            return return_vector(eta2);
        },
        R"(calculates eta2x2)", py::arg("clusters"));

    m.def(
        "calculate_eta2",
        [](const aare::Cluster<Type, CoordSizeX, CoordSizeY, CoordType>
               &cluster) { return calculate_eta2(cluster); },
        R"(calculates eta2x2)", py::arg("cluster"));

    m.def(
        "calculate_full_eta2",
        [](const aare::Cluster<Type, CoordSizeX, CoordSizeY, CoordType>
               &cluster) { return calculate_full_eta2(cluster); },
        R"(calculates full eta2x2)", py::arg("cluster"));

    m.def(
        "calculate_full_eta2",
        [](const aare::ClusterVector<ClusterType> &clusters) {
            auto eta2 = new std::vector<Eta2<typename ClusterType::value_type>>(
                calculate_full_eta2(clusters));
            return return_vector(eta2);
        },
        R"(calculates full eta2x2)", py::arg("clusters"));
}

template <typename Type, typename CoordType = uint16_t>
void register_calculate_3x3eta(py::module &m) {
    using ClusterType = Cluster<Type, 3, 3, CoordType>;

    m.def(
        "calculate_eta3",
        [](const aare::ClusterVector<ClusterType> &clusters) {
            auto eta = new std::vector<Eta2<Type>>(calculate_eta3(clusters));
            return return_vector(eta);
        },
        R"(calculates eta3x3 using entire cluster)", py::arg("clusters"));

    m.def(
        "calculate_cross_eta3",
        [](const aare::ClusterVector<ClusterType> &clusters) {
            auto eta =
                new std::vector<Eta2<Type>>(calculate_cross_eta3(clusters));
            return return_vector(eta);
        },
        R"(calculates eta3x3 taking into account cross pixels in cluster)",
        py::arg("clusters"));

    m.def(
        "calculate_eta3",
        [](const ClusterType &cluster) { return calculate_eta3(cluster); },
        R"(calculates eta3x3 using entire cluster)", py::arg("cluster"));

    m.def(
        "calculate_cross_eta3",
        [](const ClusterType &cluster) {
            return calculate_cross_eta3(cluster);
        },
        R"(calculates eta3x3 taking into account cross pixels in cluster)",
        py::arg("cluster"));
}