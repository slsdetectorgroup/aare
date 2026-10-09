// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/GapPixels.hpp"
#include "aare/ROI.hpp"
#include "aare/defs.hpp"
#include "np_helper.hpp"

#include <fmt/format.h>
#include <optional>
#include <pybind11/numpy.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace ::aare;

namespace {

template <typename T>
py::array insert_gap_pixels_typed(const py::array &image, const ROI &roi,
                                  DetectorType detector, bool quad,
                                  const GapPixels &gaps) {
    py::array_t<T, py::array::c_style | py::array::forcecast> typed(image);
    auto source = make_const_view_2d(typed);
    auto *result = new NDArray<T, 2>(
        insert_gap_pixels<T>(source, roi, detector, quad, gaps));
    return return_image_data(result);
}

} // namespace

void define_gap_pixels_bindings(py::module &m) {
    py::class_<ModuleGaps>(m, "ModuleGaps", R"(
        Pixels inserted at each boundary between modules, replacing the chip
        gap there.
        )")
        .def(py::init<>())
        .def(py::init<ssize_t, ssize_t>(), py::arg("x"), py::arg("y"))
        .def_readwrite("x", &ModuleGaps::x)
        .def_readwrite("y", &ModuleGaps::y)
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__repr__", [](const ModuleGaps &self) {
            return fmt::format("ModuleGaps(x={}, y={})", self.x, self.y);
        });

    py::class_<GapPixels>(m, "GapPixels", R"(
        Configuration for inserting gap pixels into Jungfrau and Eiger
        images. Chip and module sizes are fixed by the detector type.

        Parameters
        ----------
        chip_gap : int
            Pixels inserted at each chip boundary inside a module. Source
            column 255 expands to the right and column 256 to the left.
        module_gaps : ModuleGaps, optional
            Pixels inserted at module boundaries instead of the chip gap.
            None means module boundaries receive the chip gap.
        fill_value : float
            Value written to gap pixels, converted to the pixel type. Module
            gaps always receive this value.
        split_counts : bool
            Split the counts of the double-size pixels at chip boundaries
            between the pixel and its gap pixel. Integer remainders go to
            one side at random. Requires a chip gap of 2.
        seed : int, optional
            Seed for the random assignment of remainders.
        )")
        .def(
            py::init([](ssize_t chip_gap, std::optional<ModuleGaps> module_gaps,
                        double fill_value, bool split_counts,
                        std::optional<uint64_t> seed) {
                GapPixels gaps;
                gaps.chip_gap = chip_gap;
                gaps.module_gaps = module_gaps;
                gaps.fill_value = fill_value;
                gaps.split_counts = split_counts;
                gaps.seed = seed;
                return gaps;
            }),
            py::arg("chip_gap") = 2, py::arg("module_gaps") = py::none(),
            py::arg("fill_value") = 0.0, py::arg("split_counts") = false,
            py::arg("seed") = py::none())
        .def_readwrite("chip_gap", &GapPixels::chip_gap)
        .def_readwrite("module_gaps", &GapPixels::module_gaps)
        .def_readwrite("fill_value", &GapPixels::fill_value)
        .def_readwrite("split_counts", &GapPixels::split_counts)
        .def_readwrite("seed", &GapPixels::seed)
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__repr__", [](const GapPixels &self) {
            return fmt::format(
                "GapPixels(chip_gap={}, module_gaps={}, fill_value={}, "
                "split_counts={}, seed={})",
                self.chip_gap,
                self.module_gaps
                    ? fmt::format("ModuleGaps(x={}, y={})", self.module_gaps->x,
                                  self.module_gaps->y)
                    : "None",
                self.fill_value, self.split_counts ? "True" : "False",
                self.seed ? fmt::format("{}", *self.seed) : "None");
        });

    m.def(
        "gapped_shape",
        [](const ROI &roi, DetectorType detector_type, const GapPixels &gaps,
           bool quad) {
            const auto shape = gapped_shape(roi, detector_type, quad, gaps);
            return py::make_tuple(shape[0], shape[1]);
        },
        py::arg("roi"), py::arg("detector_type"), py::arg("gaps") = GapPixels{},
        py::arg("quad") = false, R"(
        Shape (rows, cols) of the gapped image of a detector coordinate ROI.
        )");

    m.def(
        "insert_gap_pixels",
        [](const py::array &image, DetectorType detector_type,
           const GapPixels &gaps, std::optional<ROI> roi,
           bool quad) -> py::array {
            if (image.ndim() != 2) {
                throw py::value_error(
                    fmt::format("Expected a 2D image, got {}D", image.ndim()));
            }
            const ROI rect =
                roi.value_or(ROI{0, image.shape(1), 0, image.shape(0)});
            if (py::isinstance<py::array_t<uint8_t>>(image)) {
                return insert_gap_pixels_typed<uint8_t>(
                    image, rect, detector_type, quad, gaps);
            }
            if (py::isinstance<py::array_t<uint16_t>>(image)) {
                return insert_gap_pixels_typed<uint16_t>(
                    image, rect, detector_type, quad, gaps);
            }
            if (py::isinstance<py::array_t<uint32_t>>(image)) {
                return insert_gap_pixels_typed<uint32_t>(
                    image, rect, detector_type, quad, gaps);
            }
            if (py::isinstance<py::array_t<int32_t>>(image)) {
                return insert_gap_pixels_typed<int32_t>(
                    image, rect, detector_type, quad, gaps);
            }
            if (py::isinstance<py::array_t<float>>(image)) {
                return insert_gap_pixels_typed<float>(
                    image, rect, detector_type, quad, gaps);
            }
            if (py::isinstance<py::array_t<double>>(image)) {
                return insert_gap_pixels_typed<double>(
                    image, rect, detector_type, quad, gaps);
            }
            throw py::type_error(
                "Unsupported dtype, expected uint8, uint16, uint32, int32, "
                "float32 or float64");
        },
        py::arg("image"), py::arg("detector_type"),
        py::arg("gaps") = GapPixels{}, py::arg("roi") = py::none(),
        py::arg("quad") = false, R"(
        Insert gap pixels into a 2D image.

        Parameters
        ----------
        image : numpy.ndarray
            Ungapped image of ``roi``.
        detector_type : DetectorType
            Jungfrau or Eiger.
        gaps : GapPixels
            Gap configuration.
        roi : ROI, optional
            Detector coordinates of the image. Defaults to the full image
            starting at pixel (0, 0).
        quad : bool
            True for an Eiger quad.

        Returns
        -------
        numpy.ndarray
            New array with the same dtype as ``image``.
        )");
}
