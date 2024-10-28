
#include "file.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(_aare, m) {

    

    define_file_io_bindings(m);
}