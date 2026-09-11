#include <pybind11/pybind11.h>

#include "aare/StrixelPixelRemapping/InclusiveROI.hpp"

namespace py = pybind11;

void define_InclusiveROI(py::module &m) {
    py::class_<aare::InclusiveROI>(m, "InclusiveROI")
        .def(py::init<int, int, int, int>(), py::arg("xmin"), py::arg("xmax"),
             py::arg("ymin"), py::arg("ymax"))
        .def_readwrite("xmin", &aare::InclusiveROI::xmin,
                       "minimum x coordinate (inclusive)")
        .def_readwrite("xmax", &aare::InclusiveROI::xmax,
                       "maximum x coordinate (inclusive)")
        .def_readwrite("ymin", &aare::InclusiveROI::ymin,
                       "minimum y coordinate (inclusive)")
        .def_readwrite("ymax", &aare::InclusiveROI::ymax,
                       "maximum y coordinate (inclusive)")

        .def_property_readonly("width", &aare::InclusiveROI::width,
                               "width of the ROI")

        .def_property_readonly("height", &aare::InclusiveROI::height,
                               "height of the ROI")

        .def_property_readonly("size", &aare::InclusiveROI::size,
                               "number of pixels in the ROI")

        .def("is_empty", &aare::InclusiveROI::is_empty,
             "check if the ROI is empty")

        .def(
            "contains",
            [](const aare::InclusiveROI &self, int x, int y) {
                return self.contains(x, y);
            },
            R"(
        check if a point is contained in the ROI

        Parameters
        ----------
        x : int
            x coordinate of the point
        y : int
            y coordinate of the point

        Returns
        -------
        bool
            True if the point is contained in the ROI, False otherwise
        )")

        .def(
            "fits_in",
            [](const aare::InclusiveROI &self, int ncols, int nrows) {
                return self.fits_in(ncols, nrows);
            },
            R"(
            check if the ROI fits within a given number of columns and rows
            Parameters
            ----------
            ncols : int
                number of columns
            nrows : int
                number of rows
            Returns
            -------
            bool
                True if the ROI fits within the given dimensions, False otherwise
            )")

        .def("__eq__", &aare::InclusiveROI::operator==, py::is_operator(),
             "check if two InclusiveROI objects are equal")

        .def("__repr__",
             [](const aare::InclusiveROI &self) {
                 return fmt::format(
                     "InclusiveROI(xmin={}, xmax={}, ymin={}, ymax={})",
                     self.xmin, self.xmax, self.ymin, self.ymax);
             })

        .def_static("emptyROI", &aare::InclusiveROI::emptyROI,
                    "create an empty InclusiveROI");

    m.def("toInclusiveROI", &toInclusiveROI, py::arg("roi").noconvert(),
          R"(
        Convert a half-open ROI to an inclusive ROI

        Parameters
        ----------
        roi : ROI
            Half-open ROI to be converted

        Returns
        -------
        InclusiveROI
            Inclusive ROI with the same physical extent
        )");

    m.def("toHalfopenROI", &toHalfopenROI, py::arg("roi").noconvert(),
          R"(
        Convert an inclusive ROI to a half-open ROI

        Parameters
        ----------
        roi : InclusiveROI
            Inclusive ROI to be converted

        Returns
        -------
        ROI
            Half-open ROI with the same physical extent
        )");
}