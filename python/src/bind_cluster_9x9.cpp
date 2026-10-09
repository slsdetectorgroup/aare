// SPDX-License-Identifier: MPL-2.0
#include "bind_cluster_common.hpp"

void bind_cluster_types_9x9(py::module &m) {
    DEFINE_CLUSTER_BINDINGS(int, 9, 9, uint16_t, i);
    DEFINE_CLUSTER_BINDINGS(double, 9, 9, uint16_t, d);
    DEFINE_CLUSTER_BINDINGS(float, 9, 9, uint16_t, f);
}

void bind_cluster_finders_9x9(py::module &m) {
    DEFINE_BINDINGS_CLUSTERFINDER(int, 9, 9, uint16_t, i);
    DEFINE_BINDINGS_CLUSTERFINDER(double, 9, 9, uint16_t, d);
    DEFINE_BINDINGS_CLUSTERFINDER(float, 9, 9, uint16_t, f);
}

void bind_cluster_vector_reductions_9x9(py::module &m) {
    define_3x3_reduction<int, 9, 9, uint16_t>(m);
    define_3x3_reduction<double, 9, 9, uint16_t>(m);
    define_3x3_reduction<float, 9, 9, uint16_t>(m);
}

void bind_cluster_reductions_9x9(py::module &m) {
    reduce_to_3x3<int, 9, 9, uint16_t>(m);
    reduce_to_3x3<double, 9, 9, uint16_t>(m);
    reduce_to_3x3<float, 9, 9, uint16_t>(m);
}
