// SPDX-License-Identifier: MPL-2.0
#include "bindings.hpp"

#include "aare/Dequantize.hpp"
#include "aare/GainMap.hpp"
#include "np_helper.hpp"

#include <cstdint>
#include <optional>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

using namespace aare;

// The class is registered here; bind_cluster_NxN.cpp adds one __call__
// overload per integer cluster type through register_dequantize.
void define_dequantizer_bindings(py::module &m) {
    py::class_<Dequantizer>(m, "Dequantizer",
                            R"doc(
        Converts integer clusters to float32 and replaces every pixel value
        with a uniformly distributed value from the interval that was stored
        as that integer. Optionally applies a gain in the same pass.

        Integer pixel values make ratios such as eta take only rational
        values, which shows as spikes in finely binned eta histograms and as
        lines in interpolated images. Convert the clusters before calculating
        eta and before interpolating.

        Create one Dequantizer and call it for every chunk of a file. It
        keeps the state of its random generator between calls, so the result
        does not depend on the chunk size. Do not call one object from
        several threads at the same time.

        Parameters
        ----------
        gain : float or numpy.ndarray, optional
            Gain in ADU/energy that the spread values are divided by: one
            number for all pixels, or a two-dimensional array indexed as
            ``[y, x]``. The array is copied. Clusters that extend beyond the
            array are kept with all pixel values set to zero.
        truncated : bool, default False
            How the pixel values were converted to integers. ``False`` for
            values rounded to the nearest integer, as ClusterFinder writes
            them: ``n`` is spread over ``[n - 0.5, n + 0.5)``. ``True`` for
            values truncated toward zero, as in older cluster files:
            positive ``n`` is spread over ``[n, n + 1)``, negative ``n`` over
            ``(n - 1, n]`` and 0 over ``(-1, 1)``. In truncated data the
            pixel value 0 occurs about twice as often as 1 or -1.
        seed : int, optional
            Seed of the random generator. ``None`` seeds it from the system.
        )doc")
        .def(
            py::init([](const py::object &gain, const bool truncated,
                        const std::optional<uint64_t> seed) {
                const auto quantization =
                    truncated ? Quantization::Truncated : Quantization::Rounded;
                if (gain.is_none())
                    return Dequantizer(quantization, seed);

                const auto gain_array =
                    py::array_t<double, py::array::c_style |
                                            py::array::forcecast>::ensure(gain);
                if (!gain_array) {
                    throw py::type_error(
                        "gain must be a number or a 2D array of numbers");
                }
                if (gain_array.ndim() == 0)
                    return Dequantizer(*gain_array.data(), quantization, seed);
                if (gain_array.ndim() != 2) {
                    throw py::value_error(
                        "gain must be a number or a 2D array of numbers");
                }
                // A const view also takes arrays that are not writeable
                return Dequantizer(InvertedGainMap(NDArray<double, 2>(
                                       make_const_view_2d(gain_array))),
                                   quantization, seed);
            }),
            py::kw_only(), py::arg("gain") = py::none(),
            py::arg("truncated") = false, py::arg("seed") = py::none());
}
