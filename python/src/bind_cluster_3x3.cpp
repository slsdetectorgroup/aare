// SPDX-License-Identifier: MPL-2.0
#include "bind_cluster_common.hpp"

void bind_cluster_types_3x3(py::module &m) {
    DEFINE_CLUSTER_BINDINGS(int, 3, 3, uint16_t, i);
    DEFINE_CLUSTER_BINDINGS(double, 3, 3, uint16_t, d);
    DEFINE_CLUSTER_BINDINGS(float, 3, 3, uint16_t, f);
}

void bind_cluster_types_3x3_i16(py::module &m) {
    DEFINE_CLUSTER_BINDINGS(int16_t, 3, 3, uint16_t, i16);
}

void bind_cluster_finders_3x3(py::module &m) {
    DEFINE_BINDINGS_CLUSTERFINDER(int, 3, 3, uint16_t, i);
    DEFINE_BINDINGS_CLUSTERFINDER(double, 3, 3, uint16_t, d);
    DEFINE_BINDINGS_CLUSTERFINDER(float, 3, 3, uint16_t, f);
    DEFINE_BINDINGS_CLUSTERFINDER(int16_t, 3, 3, uint16_t, i16);
}

void bind_eta_3x3(py::module &m) {
    register_calculate_3x3eta<int, 3, 3, uint16_t>(m);
    register_calculate_3x3eta<double, 3, 3, uint16_t>(m);
    register_calculate_3x3eta<float, 3, 3, uint16_t>(m);
    register_calculate_3x3eta<int16_t, 3, 3, uint16_t>(m);
}
