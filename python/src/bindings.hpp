// SPDX-License-Identifier: MPL-2.0
// Entry points of the binding translation units. Each bind_*.cpp defines the
// entry points listed under its name; module.cpp calls them in a fixed order.
// The templated cluster and eta bindings are shared headers (bind_Cluster*.hpp,
// bind_Eta.hpp) instantiated per size in bind_cluster_NxN.cpp.
#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// bind_File.cpp
void define_file_io_bindings(py::module &m);
// bind_RawFile.cpp
void define_raw_file_io_bindings(py::module &m);
// bind_RawSubFile.cpp
void define_raw_sub_file_io_bindings(py::module &m);
// bind_RawMasterFile.cpp
void define_raw_master_file_bindings(py::module &m);
// bind_CtbRawFile.cpp
void define_ctb_raw_file_io_bindings(py::module &m);
// bind_JungfrauDataFile.cpp
void define_jungfrau_data_file_io_bindings(py::module &m);
// bind_MultiThreadedFileReader.cpp
void define_multi_threaded_file_reader_bindings(py::module &m);
// bind_DataSetAggregation.cpp
void bind_dataset_aggregation(py::module &m);

// bind_Defs.cpp
void define_defs_bindings(py::module &m);
// bind_GapPixels.cpp
void define_gap_pixels_bindings(py::module &m);
// bind_PixelMap.cpp
void define_pixel_map_bindings(py::module &m);
// bind_testing.cpp
void define_testing_bindings(py::module &m);

// bind_PixelHistogram.cpp
void define_pixel_histogram_bindings(py::module &m);
// bind_PedestalTrackingPixelHistogram.cpp
void define_pedestal_tracking_pixel_histogram_bindings(py::module &m);

// bind_Pedestal.cpp
void bind_pedestals(py::module &m);
// bind_FastPedestal.cpp
void bind_fast_pedestals(py::module &m);
// bind_StateFile.cpp
void bind_state_files(py::module &m);
// bind_calibration.cpp
void bind_calibration(py::module &m);

// bind_Fit.cpp
void define_fit_bindings(py::module &m);
// bind_Interpolator.cpp
void define_interpolation_bindings(py::module &m);

// bind_VarClusterFinder.cpp
void define_var_cluster_finder_bindings(py::module &m);

// bind_Eta.cpp
void bind_eta_types(py::module &m);

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
