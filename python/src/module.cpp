//Files with bindings to the different classes
#include "file.hpp"
#include "raw_file.hpp"
#include "raw_master_file.hpp"
#include "var_cluster.hpp"
#include "pixel_map.hpp"
#include "pedestal.hpp"
#include "cluster.hpp"

//Pybind stuff
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(_aare, m) {
    define_file_io_bindings(m);
    define_raw_file_io_bindings(m);
    define_raw_master_file_bindings(m);
    define_var_cluster_finder_bindings(m);
    define_pixel_map_bindings(m);
    define_pedestal_bindings<double>(m, "Pedestal");
    define_pedestal_bindings<float>(m, "Pedestal_float32");
    define_cluster_finder_bindings(m);
}