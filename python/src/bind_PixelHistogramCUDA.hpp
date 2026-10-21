// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include "aare/hist/PixelHistogramCUDA.hpp"
#include "np_helper.hpp"

#include <cstddef>
#include <cstdint>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <string>
#include <type_traits>

namespace py = pybind11;

namespace aare {

namespace detail {

// Frames are copied to the GPU as given, so a wrong dtype must not be
// converted silently: the converted copy would be a temporary, not the
// caller's (possibly pinned) buffer. Ndim and contiguity are checked when the
// view is made.
template <typename InputType>
py::array_t<InputType> require_dtype(const py::array &arr) {
    if (!py::isinstance<py::array_t<InputType>>(arr)) {
        const auto want =
            py::str(py::dtype::of<InputType>()).cast<std::string>();
        const auto got = py::str(arr.dtype()).cast<std::string>();
        throw py::type_error(
            "PixelHistogramCUDA was built for " + want + " frames, got " + got +
            ". Build it with dtype=" + got +
            ", or convert the frames with .astype(np." + want + ").");
    }
    return py::reinterpret_borrow<py::array_t<InputType>>(arr);
}

// An output histogram must be the storage dtype, shape (rows, cols, n_bins),
// C-contiguous and writeable: values(out) copies straight into it.
template <typename StorageType>
py::array_t<StorageType> require_output(const py::array &arr, size_t rows,
                                        size_t cols, size_t n_bins) {
    const auto want = py::str(py::dtype::of<StorageType>()).cast<std::string>();
    if (!py::isinstance<py::array_t<StorageType>>(arr))
        throw py::type_error("out must have dtype " + want + ", got " +
                             py::str(arr.dtype()).cast<std::string>() + ".");
    if (arr.ndim() != 3 || static_cast<size_t>(arr.shape(0)) != rows ||
        static_cast<size_t>(arr.shape(1)) != cols ||
        static_cast<size_t>(arr.shape(2)) != n_bins)
        throw py::value_error("out must have shape (" + std::to_string(rows) +
                              ", " + std::to_string(cols) + ", " +
                              std::to_string(n_bins) + ").");
    if (!(arr.flags() & py::array::c_style))
        throw py::value_error("out must be C-contiguous.");
    if (!arr.writeable())
        throw py::value_error("out must be writeable.");
    return py::reinterpret_borrow<py::array_t<StorageType>>(arr);
}

} // namespace detail

template <typename StorageType, typename InputType>
void define_PixelHistogramCUDA(py::module &m, const std::string &class_name) {
    // 64-bit device counters for 8-byte storage, so counts can't wrap at
    // 2^32 before the cap in values().
    using DeviceStorageType =
        std::conditional_t<sizeof(StorageType) == 8, uint64_t, uint32_t>;
    using Hist =
        cuda::PixelHistogramCUDA<StorageType, DeviceStorageType, InputType>;
    using AxisType = typename Hist::AxisType;

    py::class_<Hist>(m, class_name.c_str(),
                     R"(Per-pixel histograms filled on the GPU.

             Frames are copied to the device with the dtype they have; the
             histogram axis is floating point. Build instances with the
             aare.PixelHistogramCUDA() factory rather than by class name.)")
        .def(py::init<size_t, size_t, size_t, AxisType, AxisType, size_t>(),
             py::kw_only(), py::arg("rows"), py::arg("cols"), py::arg("n_bins"),
             py::arg("xmin"), py::arg("xmax"), py::arg("batch_size"))

        .def(
            "register_input_buffer",
            [](Hist &self, const py::array &frames) {
                auto arr = detail::require_dtype<InputType>(frames);
                if (!(arr.flags() & py::array::c_style))
                    throw py::value_error("Array is not C-contiguous. Use "
                                          "np.ascontiguousarray(arr)");
                self.register_input_buffer(
                    const_cast<void *>(static_cast<const void *>(arr.data())),
                    static_cast<size_t>(arr.nbytes()));
            },
            py::arg("frames"), py::keep_alive<1, 2>(),
            R"(Pin `frames` so that fill_histogram() copies it at full DMA
             bandwidth and overlaps the copies with the kernels.

             Pass the whole dataset once; slices of it passed to
             fill_histogram() benefit too. The array is kept alive for as
             long as the histogram exists. Registering another array releases
             the previous one.)")

        .def("unregister_input_buffer", &Hist::unregister_input_buffer,
             "Release the buffer pinned by register_input_buffer().")

        .def(
            "fill_histogram",
            [](Hist &self, const py::array &frames) {
                auto arr = detail::require_dtype<InputType>(frames);
                auto view = make_const_view_3d(arr);
                py::gil_scoped_release release;
                self.fill_histogram(view);
            },
            py::arg("frames"),
            R"(Queue frames of shape (n_frames, rows, cols) for histogramming,
             in batches of batch_size.

             Returns once the work is queued, not when it is done. If the
             frames lie in a registered buffer, the copies run in the
             background: don't modify them until flush() or values() has
             returned.)")

        .def("flush", &Hist::flush, py::call_guard<py::gil_scoped_release>(),
             "Block until every queued frame has been histogrammed.")

        .def(
            "register_output_buffer",
            [](Hist &self, const py::array &out) {
                auto arr = detail::require_output<StorageType>(
                    out, self.rows(), self.cols(), self.n_bins());
                self.register_output_buffer(arr.mutable_data(),
                                            static_cast<size_t>(arr.nbytes()));
            },
            py::arg("out"), py::keep_alive<1, 2>(),
            R"(Pin `out` so that values(out=out) copies into it at full DMA
             bandwidth. `out` must have the storage dtype and shape
             (rows, cols, n_bins). It is kept alive for as long as the
             histogram exists. Registering another array releases the
             previous one.)")

        .def("unregister_output_buffer", &Hist::unregister_output_buffer,
             "Release the buffer pinned by register_output_buffer().")

        .def(
            "values",
            [](const Hist &self, const py::object &out) -> py::object {
                if (out.is_none()) {
                    NDArray<StorageType, 3> *ptr = nullptr;
                    {
                        py::gil_scoped_release release;
                        ptr = new NDArray<StorageType, 3>(self.values());
                    }
                    return return_image_data(ptr);
                }
                if (!py::isinstance<py::array>(out))
                    throw py::type_error("out must be a numpy array.");
                auto arr = detail::require_output<StorageType>(
                    py::reinterpret_borrow<py::array>(out), self.rows(),
                    self.cols(), self.n_bins());
                auto view = make_view_3d(arr);
                {
                    py::gil_scoped_release release;
                    self.values(view);
                }
                return out;
            },
            py::arg("out") = py::none(),
            R"(Histogram of every queued frame, shape (rows, cols, n_bins).
             Counts are capped at the storage dtype's maximum. Waits for the
             queued work; filling can continue afterwards.

             With `out`, the histogram is written into that array, which is
             returned. Reusing one array, pinned with
             register_output_buffer(), avoids allocating and copying through
             pageable memory on every call.)")

        .def(
            "values_pixel",
            [](const Hist &self, size_t row, size_t col) {
                NDArray<StorageType, 1> *ptr = nullptr;
                {
                    py::gil_scoped_release release;
                    ptr = new NDArray<StorageType, 1>(
                        self.values_pixel(row, col));
                }
                return return_image_data(ptr);
            },
            py::arg("row"), py::arg("col"),
            R"(Histogram of one pixel, shape (n_bins,). For many pixels, call
             values() once instead. Raises IndexError outside the frame.)")

        .def(
            "bin_edges",
            [](const Hist &self) {
                return return_image_data(
                    new NDArray<AxisType, 1>(self.bin_edges()));
            },
            "Bin edges, n_bins + 1 values from xmin to xmax.")

        .def(
            "bin_centers",
            [](const Hist &self) {
                return return_image_data(
                    new NDArray<AxisType, 1>(self.bin_centers()));
            },
            "Bin centres, n_bins values.")

        .def_property_readonly("rows", &Hist::rows)
        .def_property_readonly("cols", &Hist::cols)
        .def_property_readonly("n_bins", &Hist::n_bins)
        .def_property_readonly("batch_size", &Hist::batch_size)
        .def_property_readonly(
            "dtype", [](const Hist &) { return py::dtype::of<InputType>(); },
            "dtype of the frames this histogram takes.");
}

// Class names are PixelHistogramCUDA_<input>_<storage>, e.g.
// PixelHistogramCUDA_u16_u32 for uint16 frames and uint32 counts.
template <typename InputType>
void define_PixelHistogramCUDA_storages(py::module &m,
                                        const std::string &input) {
    const std::string base = "PixelHistogramCUDA_" + input + "_";
    define_PixelHistogramCUDA<uint8_t, InputType>(m, base + "u8");
    define_PixelHistogramCUDA<uint16_t, InputType>(m, base + "u16");
    define_PixelHistogramCUDA<uint32_t, InputType>(m, base + "u32");
    define_PixelHistogramCUDA<uint64_t, InputType>(m, base + "u64");
    define_PixelHistogramCUDA<float, InputType>(m, base + "f");
    define_PixelHistogramCUDA<double, InputType>(m, base + "d");
}

inline void define_PixelHistogramCUDA_bindings(py::module &m) {
    define_PixelHistogramCUDA_storages<uint16_t>(m, "u16");
    define_PixelHistogramCUDA_storages<float>(m, "f");
    define_PixelHistogramCUDA_storages<double>(m, "d");
}

} // namespace aare
