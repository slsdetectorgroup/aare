#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "aare/defs.hpp"

namespace py = pybind11;
using namespace aare;

void define_defs_bindings(py::module &m) {
    auto matterhorn10 = py::class_<Matterhorn10>(m, "Matterhorn10");
    matterhorn10.attr("nRows") = Matterhorn10::nRows;
    matterhorn10.attr("nCols") = Matterhorn10::nCols;

    auto matterhorn02 = py::class_<Matterhorn02>(m, "Matterhorn02");
    matterhorn02.attr("nRows") = Matterhorn02::nRows;
    matterhorn02.attr("nCols") = Matterhorn02::nCols;
    matterhorn02.attr("nHalfCols") = Matterhorn02::nHalfCols;

    auto moench04 = py::class_<Moench04>(m, "Moench04");
    moench04.attr("nRows") = Moench04::nRows;
    moench04.attr("nCols") = Moench04::nCols;
    moench04.attr("nPixelsPerSuperColumn") = Moench04::nPixelsPerSuperColumn;
    moench04.attr("superColumnWidth") = Moench04::superColumnWidth;
    moench04.attr("adcNumbers") = Moench04::adcNumbers;
}
