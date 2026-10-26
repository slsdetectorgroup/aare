// SPDX-License-Identifier: MPL-2.0
#include "bind_cluster_common.hpp"

void bind_cluster_types_7x7(py::module &m) {
    DEFINE_CLUSTER_BINDINGS(int, 7, 7, uint16_t, i);
    DEFINE_CLUSTER_BINDINGS(double, 7, 7, uint16_t, d);
    DEFINE_CLUSTER_BINDINGS(float, 7, 7, uint16_t, f);
}

void bind_cluster_finders_7x7(py::module &m) {
    DEFINE_BINDINGS_CLUSTERFINDER(int, 7, 7, uint16_t, i);
    DEFINE_BINDINGS_CLUSTERFINDER(double, 7, 7, uint16_t, d);
    DEFINE_BINDINGS_CLUSTERFINDER(float, 7, 7, uint16_t, f);
}

void bind_cluster_vector_reductions_7x7(py::module &m) {
    define_3x3_reduction<int, 7, 7, uint16_t>(m);
    define_3x3_reduction<double, 7, 7, uint16_t>(m);
    define_3x3_reduction<float, 7, 7, uint16_t>(m);
}

void bind_cluster_reductions_7x7(py::module &m) {
    reduce_to_3x3<int, 7, 7, uint16_t>(m);
    reduce_to_3x3<double, 7, 7, uint16_t>(m);
    reduce_to_3x3<float, 7, 7, uint16_t>(m);
}
