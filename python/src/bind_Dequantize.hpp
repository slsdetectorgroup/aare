// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/ClusterVector.hpp"
#include "aare/Dequantize.hpp"

#include <cstdint>
#include <pybind11/pybind11.h>
#include <type_traits>

namespace py = pybind11;

using namespace aare;

// Adds Dequantizer.__call__ for an integer cluster type; does nothing for the
// floating point ones. The class is registered in bind_Dequantize.cpp.
template <typename Type, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = uint16_t>
void register_dequantize([[maybe_unused]] py::module &m) {
    if constexpr (std::is_integral_v<Type>) {
        using Input =
            ClusterVector<Cluster<Type, ClusterSizeX, ClusterSizeY, CoordType>>;

        py::reinterpret_borrow<py::class_<Dequantizer>>(m.attr("Dequantizer"))
            .def(
                "__call__",
                // Cast here: the float ClusterVector is registered after
                // the integer one, too late for the signature in the
                // docstring.
                [](Dequantizer &self, const Input &clusters) -> py::object {
                    return py::cast(self(clusters));
                },
                py::arg("clusters"),
                R"doc(
                Convert integer clusters to float32 clusters of the same
                size. The input is not changed.
                )doc");
    }
}
