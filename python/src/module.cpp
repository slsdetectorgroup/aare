//Files with bindings to the different classes
#include "file.hpp"
#include "var_cluster.hpp"
#include "pixel_map.hpp"

//Pybind stuff
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(_aare, m) {
    define_file_io_bindings(m);
    define_var_cluster_finder_bindings(m);
    define_pixel_map_bindings(m);
}