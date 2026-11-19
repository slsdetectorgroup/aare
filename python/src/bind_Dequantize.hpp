// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/ClusterVector.hpp"
#include "aare/Dequantize.hpp"
#include "aare/GainMap.hpp"
#include "aare/NDArray.hpp"
#include "np_helper.hpp"

#include <cstdint>
#include <optional>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <type_traits>

namespace py = pybind11;

using namespace aare;

// Registers dequantize for integer cluster types and does nothing for the
// floating point ones, so it can be part of DEFINE_CLUSTER_BINDINGS.
template <typename Type, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = uint16_t>
void register_dequantize([[maybe_unused]] py::module &m) {
    if constexpr (std::is_integral_v<Type>) {
        using Input =
            ClusterVector<Cluster<Type, ClusterSizeX, ClusterSizeY, CoordType>>;
        using Output = ClusterVector<
            Cluster<float, ClusterSizeX, ClusterSizeY, CoordType>>;

        m.def(
            "dequantize",
            [](const Input &clusters, py::object gain, bool truncated,
               std::optional<uint64_t> seed, py::object out) -> py::object {
                const auto quantization =
                    truncated ? Quantization::Truncated : Quantization::Rounded;
                auto generator = detail::dequantize_generator(seed);
                if (out.is_none()) {
                    out = py::cast(new Output(clusters.size()),
                                   py::return_value_policy::take_ownership);
                } else if (!py::isinstance<Output>(out)) {
                    throw py::type_error(
                        "out must be a float32 ClusterVector with the cluster "
                        "size of the input");
                }
                auto &converted = out.cast<Output &>();

                if (gain.is_none()) {
                    dequantize(clusters, converted, quantization, generator);
                    return out;
                }
                const auto gain_array =
                    py::array_t<double, py::array::c_style |
                                            py::array::forcecast>::ensure(gain);
                if (!gain_array) {
                    throw py::type_error(
                        "gain must be a number or a 2D array of numbers");
                }
                if (gain_array.ndim() == 0) {
                    dequantize(clusters, converted, quantization, generator,
                               *gain_array.data());
                } else {
                    const InvertedGainMap inverse(
                        NDArray<double, 2>(make_const_view_2d(gain_array)));
                    dequantize(clusters, converted, quantization, generator,
                               inverse);
                }
                return out;
            },
            R"doc(
            Convert integer clusters to float32 and replace every pixel value
            with a uniformly distributed value from the interval that was
            stored as that integer. Optionally apply a gain in the same pass.

            Integer pixel values make ratios such as eta take only rational
            values, which shows as spikes in finely binned eta histograms and
            as lines in interpolated images. Call this before calculating eta
            and before interpolating. Coordinates and the frame number are
            copied; the input is not changed.

            Parameters
            ----------
            clusters : ClusterVector
                Clusters with an integer pixel type.
            gain : float or numpy.ndarray, optional
                Gain in ADU/energy that the spread values are divided by:
                one number for all pixels, or a two-dimensional array
                indexed as ``[y, x]`` as for ``ClusterFile.set_gain_map``.
                Clusters that extend beyond the array are kept with all
                pixel values set to zero. Do not combine it with a gain map
                set on the ClusterFile.
            truncated : bool, default False
                How the pixel values were converted to integers. ``False``
                for values rounded to the nearest integer, as ClusterFinder
                writes them: ``n`` is spread over ``[n - 0.5, n + 0.5)``.
                ``True`` for values truncated toward zero, as in older
                cluster files: positive ``n`` is spread over ``[n, n + 1)``,
                negative ``n`` over ``(n - 1, n]`` and 0 over ``(-1, 1)``. In
                truncated data the pixel value 0 occurs about twice as often
                as 1 or -1.
            seed : int, optional
                Seed of the random generator. ``None`` seeds it from the
                system. Calls with the same seed add the same sequence, so
                use a different seed for every chunk of a file.
            out : ClusterVector, optional
                float32 ClusterVector with the same cluster size that
                receives the result. Its content is replaced and its
                allocation is reused when it is large enough. NumPy views of
                it become invalid.

            Returns
            -------
            ClusterVector
                The converted clusters; ``out`` when it was given.
            )doc",
            py::arg("clusters"), py::kw_only(), py::arg("gain") = py::none(),
            py::arg("truncated") = false, py::arg("seed") = py::none(),
            py::arg("out") = py::none());
    }
}
