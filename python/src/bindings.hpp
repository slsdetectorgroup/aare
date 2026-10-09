// SPDX-License-Identifier: MPL-2.0
// Entry points of the binding translation units. Each function registers one
// area of the module; module.cpp calls them in a fixed order.
#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// bind_io.cpp (functions defined in the individual bind_*.hpp headers)
void define_file_io_bindings(py::module &m);
void define_multi_threaded_file_reader_bindings(py::module &m);
void define_raw_file_io_bindings(py::module &m);
void define_raw_sub_file_io_bindings(py::module &m);
void define_ctb_raw_file_io_bindings(py::module &m);
void define_raw_master_file_bindings(py::module &m);
void define_var_cluster_finder_bindings(py::module &m);
void define_pixel_map_bindings(py::module &m);
void define_jungfrau_data_file_io_bindings(py::module &m);
void bind_calibration(py::module &m);
void define_testing_bindings(py::module &m);
void define_defs_bindings(py::module &m);

// bind_hist.cpp
void define_pixel_histogram_bindings(py::module &m);
void define_pedestal_tracking_pixel_histogram_bindings(py::module &m);

// bind_pedestal.cpp
void bind_pedestals(py::module &m);

// bind_fit.cpp
void define_fit_bindings(py::module &m);
void define_interpolation_bindings(py::module &m);

// bind_cluster_NxN.cpp
void bind_cluster_types_3x3(py::module &m);
void bind_cluster_types_3x3_i16(py::module &m);
void bind_cluster_types_2x2(py::module &m);
void bind_cluster_types_5x5(py::module &m);
void bind_cluster_types_7x7(py::module &m);
void bind_cluster_types_9x9(py::module &m);
void bind_cluster_finders_3x3(py::module &m);
void bind_cluster_finders_5x5(py::module &m);
void bind_cluster_finders_7x7(py::module &m);
void bind_cluster_finders_9x9(py::module &m);
void bind_cluster_vector_reductions_5x5(py::module &m);
void bind_cluster_vector_reductions_7x7(py::module &m);
void bind_cluster_vector_reductions_9x9(py::module &m);
void bind_cluster_reductions_5x5(py::module &m);
void bind_cluster_reductions_7x7(py::module &m);
void bind_cluster_reductions_9x9(py::module &m);
void bind_eta_3x3(py::module &m);

// bind_eta.cpp
void bind_eta_types(py::module &m);
