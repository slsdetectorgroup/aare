// SPDX-License-Identifier: MPL-2.0
#include "aare/CalculateEta.hpp"
#include "aare/Interpolator.hpp"
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include "np_helper.hpp"
#include <cstdint>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#define REGISTER_INTERPOLATOR_ETA2(T, N, M, U)                                 \
    register_interpolate<T, N, M, U, aare::calculate_full_eta2<T, N, M, U>>(   \
        interpolator, "_full_eta2", "full eta2");                              \
    register_interpolate<T, N, M, U, aare::calculate_eta2<T, N, M, U>>(        \
        interpolator, "", "eta2");

#define REGISTER_INTERPOLATOR_ETA3(T, N, M, U)                                 \
    register_interpolate<T, N, M, U, aare::calculate_eta3<T, N, M, U>>(        \
        interpolator, "_eta3", "full eta3");                                   \
    register_interpolate<T, N, M, U, aare::calculate_cross_eta3<T, N, M, U>>(  \
        interpolator, "_cross_eta3", "cross eta3");

template <typename Type, uint8_t CoordSizeX, uint8_t CoordSizeY,
          typename CoordType = uint16_t, auto EtaFunction>
void register_interpolate(py::class_<aare::Interpolator> &interpolator,
                          const std::string &typestr = "",
                          const std::string &doc_string_etatype = "eta2x2") {

    using ClusterType = Cluster<Type, CoordSizeX, CoordSizeY, CoordType>;

    const std::string docstring = "interpolation based on " +
                                  doc_string_etatype +
                                  "\n\nReturns:\n interpolated photons";

    auto function_name = fmt::format("interpolate{}", typestr);

    interpolator.def(
        function_name.c_str(),
        [](aare::Interpolator &self,
           const ClusterVector<ClusterType> &clusters) {
            auto photons = self.interpolate<EtaFunction, ClusterType>(clusters);
            auto *ptr = new std::vector<Photon>{photons};
            return return_vector(ptr);
        },
        docstring.c_str(), py::arg("cluster_vector"));
}

template <typename Type>
void register_transform_eta_values(
    py::class_<aare::Interpolator> &interpolator) {
    interpolator.def(
        "transform_eta_values",
        [](Interpolator &self, const Eta2<Type> &eta) {
            auto uniform_coord = self.transform_eta_values(eta);
            return py::make_tuple(uniform_coord.x, uniform_coord.y);
        },
        R"(eta.x eta.y transformed to uniform coordinates based on CDF ietax, ietay)",
        py::arg("Eta"));
}

void define_interpolation_bindings(py::module &m) {

    PYBIND11_NUMPY_DTYPE(aare::Photon, x, y, energy);

    auto interpolator =
        py::class_<aare::Interpolator>(m, "Interpolator")
            .def(py::init(
                [](py::array_t<double,
                               py::array::c_style | py::array::forcecast>
                       etacube,
                   py::array_t<double> xbins, py::array_t<double> ybins,
                   py::array_t<double> ebins) {
                    return Interpolator(
                        make_view_3d(etacube), make_view_1d(xbins),
                        make_view_1d(ybins), make_view_1d(ebins));
                }), 
                R"doc(
                Constructor 

                Args:
                
                etacube: 
                    joint distribution of eta_x, eta_y and photon energy (**Note:** for the joint distribution first dimension is eta_x, second: eta_y, third: energy bins.)
                xbins: 
                    bin edges of etax
                ybins: 
                    bin edges of etay
                ebins: 
                    bin edges of photon energy
                )doc",
                py::arg("etacube"),
                py::arg("xbins"), py::arg("ybins"),
                py::arg("ebins"))

            .def(py::init(
                [](py::array_t<double> xbins, py::array_t<double> ybins,
                   py::array_t<double> ebins) {
                    return Interpolator(make_view_1d(xbins),
                                        make_view_1d(ybins),
                                        make_view_1d(ebins));
                }),
                R"(
                Constructor

                Args: 
                
                xbins: 
                    bin edges of etax
                ybins: 
                    bin edges of etay
                ebins: 
                    bin edges of photon energy
                )", py::arg("xbins"),
                py::arg("ybins"), py::arg("ebins"))
            .def(
                "rosenblatttransform",
                [](Interpolator &self,
                   py::array_t<double,
                               py::array::c_style | py::array::forcecast>
                       etacube) {
                    return self.rosenblatttransform(make_view_3d(etacube));
                },
                R"(
                calculated the rosenblatttransform for the given distribution
                
                etacube: 
                    joint distribution of eta_x, eta_y and photon energy (**Note:** for the joint distribution first dimension is eta_x, second: eta_y, third: energy bins.)
                )",
                py::arg("etacube"))
            .def("get_ietax",
                 [](Interpolator &self) {
                     auto *ptr = new NDArray<double, 3>{};
                     *ptr = self.get_ietax();
                     return return_image_data(ptr);
                 }, R"(conditional CDF of etax conditioned on etay, marginal CDF of etax (if rosenblatt transform applied))")
            .def("get_ietay", [](Interpolator &self) {
                auto *ptr = new NDArray<double, 3>{};
                *ptr = self.get_ietay();
                return return_image_data(ptr);
            }, R"(conditional CDF of etay conditioned on etax)");

    REGISTER_INTERPOLATOR_ETA3(int, 3, 3, uint16_t);
    REGISTER_INTERPOLATOR_ETA3(float, 3, 3, uint16_t);
    REGISTER_INTERPOLATOR_ETA3(double, 3, 3, uint16_t);

    REGISTER_INTERPOLATOR_ETA2(int, 3, 3, uint16_t);
    REGISTER_INTERPOLATOR_ETA2(float, 3, 3, uint16_t);
    REGISTER_INTERPOLATOR_ETA2(double, 3, 3, uint16_t);
    REGISTER_INTERPOLATOR_ETA2(int, 2, 2, uint16_t);
    REGISTER_INTERPOLATOR_ETA2(float, 2, 2, uint16_t);
    REGISTER_INTERPOLATOR_ETA2(double, 2, 2, uint16_t);

    register_transform_eta_values<int>(interpolator);
    register_transform_eta_values<float>(interpolator);
    register_transform_eta_values<double>(interpolator);

    // TODO! Evaluate without converting to double
    m.def(
        "hej",
        []() {
            // auto boost_histogram = py::module_::import("boost_histogram");
            // py::object axis =
            // boost_histogram.attr("axis").attr("Regular")(10, 0.0, 10.0);
            // py::object histogram = boost_histogram.attr("Histogram")(axis);
            // return histogram;
            // return h;
        },
        R"(
        Evaluate a 1D Gaussian function for all points in x using parameters par.

        Parameters
        ----------
        x : array_like
            The points at which to evaluate the Gaussian function.
        par : array_like
            The parameters of the Gaussian function. The first element is the amplitude, the second element is the mean, and the third element is the standard deviation.
        )");
}