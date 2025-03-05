#include "aare/Interpolator.hpp"
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include "np_helper.hpp"
#include <cstdint>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
void define_interpolation_bindings(py::module &m) {

    PYBIND11_NUMPY_DTYPE(aare::Photon, x,y,energy);

    py::class_<aare::Interpolator>(m, "Interpolator")
        .def(py::init([](py::array_t<double, py::array::c_style | py::array::forcecast> etacube, py::array_t<double> xbins,
                         py::array_t<double> ybins, py::array_t<double> ebins) {
            return Interpolator(make_view_3d(etacube), make_view_1d(xbins),
                                make_view_1d(ybins), make_view_1d(ebins));
        }))
        .def("get_ietax", [](Interpolator& self){
            auto*ptr = new NDArray<double,3>{};
            *ptr = self.get_ietax();
            return return_image_data(ptr);
        })
        .def("get_ietay", [](Interpolator& self){
            auto*ptr = new NDArray<double,3>{};
            *ptr = self.get_ietay();
            return return_image_data(ptr);
        })
        .def("interpolate", [](Interpolator& self, const ClusterVector<int32_t>& clusters){
            auto photons = self.interpolate(clusters);
            auto* ptr = new std::vector<Photon>{photons};
            return return_vector(ptr);
        });

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