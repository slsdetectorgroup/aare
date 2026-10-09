// SPDX-License-Identifier: MPL-2.0
// Module registration. The bindings live in bind_*.cpp translation units so
// they compile in parallel; the call order below is kept as before.

#include "bindings.hpp"

#include <pybind11/pybind11.h>

PYBIND11_MODULE(_aare, m) {
    auto experimental = m.def_submodule(
        "experimental", "Experimental APIs that may change without notice");

    define_file_io_bindings(m);
    define_multi_threaded_file_reader_bindings(experimental);
    define_raw_file_io_bindings(m);
    define_raw_sub_file_io_bindings(m);
    define_ctb_raw_file_io_bindings(m);
    define_raw_master_file_bindings(m);
    define_var_cluster_finder_bindings(m);
    define_pixel_map_bindings(m);
    define_pixel_histogram_bindings(m);
    define_pedestal_tracking_pixel_histogram_bindings(m);
    bind_pedestals(m);
    define_fit_bindings(m);
    define_interpolation_bindings(m);
    define_jungfrau_data_file_io_bindings(m);

    bind_calibration(m);
    define_testing_bindings(m);

    bind_cluster_types_3x3(m);
    bind_cluster_types_2x2(m);
    bind_cluster_types_5x5(m);
    bind_cluster_types_7x7(m);
    bind_cluster_types_9x9(m);
    bind_cluster_types_3x3_i16(m);

    bind_cluster_finders_3x3(m);
    bind_cluster_finders_5x5(m);
    bind_cluster_finders_7x7(m);
    bind_cluster_finders_9x9(m);

    bind_cluster_vector_reductions_5x5(m);
    bind_cluster_vector_reductions_7x7(m);
    bind_cluster_vector_reductions_9x9(m);

    bind_cluster_reductions_5x5(m);
    bind_cluster_reductions_7x7(m);
    bind_cluster_reductions_9x9(m);

    bind_eta_3x3(m);

    define_defs_bindings(m);

    bind_eta_types(m);
}
