// SPDX-License-Identifier: MPL-2.0
#include "bind_cluster_common.hpp"

void bind_cluster_types_2x2(py::module &m) {
    DEFINE_CLUSTER_BINDINGS(int, 2, 2, uint16_t, i);
    DEFINE_CLUSTER_BINDINGS(double, 2, 2, uint16_t, d);
    DEFINE_CLUSTER_BINDINGS(float, 2, 2, uint16_t, f);
}
