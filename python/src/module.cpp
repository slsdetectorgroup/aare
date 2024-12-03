//Files with bindings to the different classes
#include "file.hpp"
#include "raw_file.hpp"
#include "ctb_raw_file.hpp"
#include "raw_master_file.hpp"
#ifdef HDF5_FOUND
#include "hdf5_file.hpp"
#include "hdf5_master_file.hpp"
#endif
#include "var_cluster.hpp"
#include "pixel_map.hpp"
#include "pedestal.hpp"
#include "cluster.hpp"
#include "cluster_file.hpp"

//Pybind stuff
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(_aare, m) {
    define_file_io_bindings(m);
    define_raw_file_io_bindings(m);
    define_ctb_raw_file_io_bindings(m);
    define_raw_master_file_bindings(m);
#ifdef HDF5_FOUND
    define_hdf5_file_io_bindings(m);
    define_hdf5_master_file_bindings(m);
#endif
    define_var_cluster_finder_bindings(m);
    define_pixel_map_bindings(m);
    define_pedestal_bindings<double>(m, "Pedestal");
    define_pedestal_bindings<float>(m, "Pedestal_float32");
    define_cluster_finder_bindings(m);
    define_cluster_file_io_bindings(m);
}