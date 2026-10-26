// SPDX-License-Identifier: MPL-2.0
#include "module_config.hpp"

#include "bind_Eta.hpp"

#include <pybind11/numpy.h>
#include <pybind11/stl.h>

void bind_eta_types(py::module &m) {
    using Sum_index_pair_d = Sum_index_pair<double, corner>;
    PYBIND11_NUMPY_DTYPE(Sum_index_pair_d, sum, index);
    using Sum_index_pair_f = Sum_index_pair<float, corner>;
    PYBIND11_NUMPY_DTYPE(Sum_index_pair_f, sum, index);
    using Sum_index_pair_i = Sum_index_pair<int, corner>;
    PYBIND11_NUMPY_DTYPE(Sum_index_pair_i, sum, index);

    using eta_d = Eta2<double>;
    PYBIND11_NUMPY_DTYPE(eta_d, x, y, c, sum);
    using eta_i = Eta2<int>;
    PYBIND11_NUMPY_DTYPE(eta_i, x, y, c, sum);
    using eta_f = Eta2<float>;
    PYBIND11_NUMPY_DTYPE(eta_f, x, y, c, sum);
    using eta_i16 = Eta2<int16_t>;
    PYBIND11_NUMPY_DTYPE(eta_i16, x, y, c, sum);

    define_corner_enum(m);
    define_eta<float>(m, "f");
    define_eta<double>(m, "d");
    define_eta<int>(m, "i");
    define_eta<int16_t>(m, "i16");
}
