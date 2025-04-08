//Files with bindings to the different classes
#include "file.hpp"
#include "raw_file.hpp"
#include "ctb_raw_file.hpp"
#include "raw_master_file.hpp"
#include "var_cluster.hpp"
#include "pixel_map.hpp"
#include "pedestal.hpp"
#include "cluster.hpp"
#include "cluster_file.hpp"
#include "fit.hpp"
#include "interpolation.hpp"

#include "jungfrau_data_file.hpp"

//Pybind stuff
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(_aare, m) {
    define_file_io_bindings(m);
    define_raw_file_io_bindings(m);
    define_ctb_raw_file_io_bindings(m);
    define_raw_master_file_bindings(m);
    define_var_cluster_finder_bindings(m);
    define_pixel_map_bindings(m);
    define_pedestal_bindings<double>(m, "Pedestal_d");
    define_pedestal_bindings<float>(m, "Pedestal_f");
    define_cluster_finder_bindings(m);
    define_cluster_finder_mt_bindings(m);
    define_cluster_file_io_bindings(m);
    define_cluster_collector_bindings(m);
    define_cluster_file_sink_bindings(m);
    define_fit_bindings(m);
    define_interpolation_bindings(m);
    define_jungfrau_data_file_io_bindings(m);

}