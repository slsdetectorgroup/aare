// SPDX-License-Identifier: MPL-2.0
#include "module_config.hpp"

#include "bind_FastPedestal.hpp"
#include "bind_Pedestal.hpp"

void bind_pedestals(py::module &m) {
    define_pedestal_bindings<double>(m, "Pedestal_d");
    define_pedestal_bindings<float>(m, "Pedestal_f");
    define_pedestal_bindings<int16_t>(m, "Pedestal_i16");
    define_fast_pedestal_bindings<double>(m, "FastPedestal_d");
    define_fast_pedestal_bindings<float>(m, "FastPedestal_f");
    define_fast_pedestal_bindings<int16_t>(m, "FastPedestal_i16");
}
