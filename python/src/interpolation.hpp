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

template <typename Type, uint8_t CoordSizeX, uint8_t CoordSizeY,
          typename CoordType = uint16_t, auto EtaFunction>
void register_interpolate(py::class_<aare::Interpolator> &interpolator,
                          const std::string &typestr = "",
                          const std::string &doc_string_etatype = "eta2x2") {

    using ClusterType = Cluster<Type, CoordSizeX, CoordSizeY, CoordType>;

    const std::string docstring = "interpolation based on" +
                                  doc_string_etatype +
                                  "\n\nReturns:\n interpolated photons";

    auto function_name = fmt::format("interpolate{}", typestr);

    interpolator.def(
        function_name.c_str(),
        [](aare::Interpolator &self,
           const ClusterVector<ClusterType> &clusters) {
            auto photons = self.interpolate<ClusterType, EtaFunction>(clusters);
            auto *ptr = new std::vector<Photon>{photons};
            return return_vector(ptr);
        },
        docstring.c_str(), py::arg("cluster_vector"));
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
                R"(
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
                )",
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

                Args: 
                
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

    register_interpolate<int, 3, 3, uint16_t,
                         aare::calculate_eta2<int, 3, 3, uint16_t>>(
        interpolator);
    register_interpolate<float, 3, 3, uint16_t,
                         aare::calculate_eta2<float, 3, 3, uint16_t>>(
        interpolator);
    register_interpolate<double, 3, 3, uint16_t,
                         aare::calculate_eta2<double, 3, 3, uint16_t>>(
        interpolator);
    register_interpolate<int, 2, 2, uint16_t,
                         aare::calculate_eta2<int, 2, 2, uint16_t>>(
        interpolator);
    register_interpolate<float, 2, 2, uint16_t,
                         aare::calculate_eta2<float, 2, 2, uint16_t>>(
        interpolator);
    register_interpolate<double, 2, 2, uint16_t,
                         aare::calculate_eta2<double, 2, 2, uint16_t>>(
        interpolator);

    register_interpolate<int, 3, 3, uint16_t, aare::calculate_eta3<int>>(
        interpolator, "_eta3x3", "eta3x3");
    register_interpolate<float, 3, 3, uint16_t, aare::calculate_eta3<float>>(
        interpolator, "_eta3x3", "eta3x3");
    register_interpolate<double, 3, 3, uint16_t, aare::calculate_eta3<double>>(
        interpolator, "_eta3x3", "eta3x3");

    register_interpolate<int, 3, 3, uint16_t, aare::calculate_cross_eta3<int>>(
        interpolator, "_cross_eta3x3", "cross eta3x3");
    register_interpolate<float, 3, 3, uint16_t,
                         aare::calculate_cross_eta3<float>>(
        interpolator, "_cross_eta3x3", "cross eta3x3");
    register_interpolate<double, 3, 3, uint16_t,
                         aare::calculate_cross_eta3<double>>(
        interpolator, "_cross_eta3x3", "cross eta3x3");

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