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

    auto moench05 = py::class_<Moench05>(m, "Moench05");
    moench05.attr("nRows") = Moench05::nRows;
    moench05.attr("nCols") = Moench05::nCols;
    moench05.attr("adcNumbers") = Moench05::adcNumbers;

    py::class_<ROI>(m, "ROI")
        .def(py::init<>())
        .def(py::init<ssize_t, ssize_t, ssize_t, ssize_t>(), py::arg("xmin"),
             py::arg("xmax"), py::arg("ymin"), py::arg("ymax"))
        .def_readwrite("xmin", &ROI::xmin)
        .def_readwrite("xmax", &ROI::xmax)
        .def_readwrite("ymin", &ROI::ymin)
        .def_readwrite("ymax", &ROI::ymax)
        .def("__str__",
             [](const ROI &self) {
                 return fmt::format("ROI: xmin: {} xmax: {} ymin: {} ymax: {}",
                                    self.xmin, self.xmax, self.ymin, self.ymax);
             })
        .def("__repr__",
             [](const ROI &self) {
                 return fmt::format(
                     "<ROI: xmin: {} xmax: {} ymin: {} ymax: {}>", self.xmin,
                     self.xmax, self.ymin, self.ymax);
             })
        .def("__iter__",
             [](const ROI &self) {
                 return py::make_iterator(&self.xmin, &self.ymax + 1); // NOLINT
             })

        .def("__eq__", [](const ROI &self, const ROI &other) {
            return self.xmin == other.xmin && self.xmax == other.xmax &&
                   self.ymin == other.ymin && self.ymax == other.ymax;
        });

    py::enum_<DetectorType>(m, "DetectorType")
        .value("Jungfrau", DetectorType::Jungfrau)
        .value("Eiger", DetectorType::Eiger)
        .value("Mythen3", DetectorType::Mythen3)
        .value("Moench", DetectorType::Moench)
        .value("Moench03", DetectorType::Moench03)
        .value("Moench03_old", DetectorType::Moench03_old)
        .value("ChipTestBoard", DetectorType::ChipTestBoard)
        .value("Unknown", DetectorType::Unknown);

    py::enum_<UDPPortPosition>(m, "UDPPortPosition")
        .value("LEFT", UDPPortPosition::LEFT)
        .value("RIGHT", UDPPortPosition::RIGHT)
        .value("TOP", UDPPortPosition::TOP)
        .value("BOTTOM", UDPPortPosition::BOTTOM);
}
