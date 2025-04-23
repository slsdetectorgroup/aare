// Files with bindings to the different classes

//New style file naming
#include "bind_ClusterVector.hpp"

//TODO! migrate the other names
#include "cluster.hpp"
#include "cluster_file.hpp"
#include "ctb_raw_file.hpp"
#include "file.hpp"
#include "fit.hpp"
#include "interpolation.hpp"
#include "raw_sub_file.hpp"
#include "raw_master_file.hpp"
#include "raw_file.hpp"
#include "pixel_map.hpp"
#include "var_cluster.hpp"
#include "pedestal.hpp"
#include "jungfrau_data_file.hpp"

// Pybind stuff
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(_aare, m) {
    define_file_io_bindings(m);
    define_raw_file_io_bindings(m);
    define_raw_sub_file_io_bindings(m);
    define_ctb_raw_file_io_bindings(m);
    define_raw_master_file_bindings(m);
    define_var_cluster_finder_bindings(m);
    define_pixel_map_bindings(m);
    define_pedestal_bindings<double>(m, "Pedestal_d");
    define_pedestal_bindings<float>(m, "Pedestal_f");
    define_fit_bindings(m);
    define_interpolation_bindings(m);
    define_jungfrau_data_file_io_bindings(m);

    define_cluster_file_io_bindings<int, 3, 3, uint16_t>(m, "Cluster3x3i");
    define_cluster_file_io_bindings<double, 3, 3, uint16_t>(m, "Cluster3x3d");
    define_cluster_file_io_bindings<float, 3, 3, uint16_t>(m, "Cluster3x3f");
    define_cluster_file_io_bindings<int, 2, 2, uint16_t>(m, "Cluster2x2i");
    define_cluster_file_io_bindings<float, 2, 2, uint16_t>(m, "Cluster2x2f");
    define_cluster_file_io_bindings<double, 2, 2, uint16_t>(m, "Cluster2x2d");

    define_ClusterVector<int, 3, 3, uint16_t>(m, "Cluster3x3i");
    define_ClusterVector<double, 3, 3, uint16_t>(m, "Cluster3x3d");
    define_ClusterVector<float, 3, 3, uint16_t>(m, "Cluster3x3f");
    define_ClusterVector<int, 2, 2, uint16_t>(m, "Cluster2x2i");
    define_ClusterVector<double, 2, 2, uint16_t>(m, "Cluster2x2d");
    define_ClusterVector<float, 2, 2, uint16_t>(m, "Cluster2x2f");

    define_cluster_finder_bindings<int, 3, 3, uint16_t>(m, "Cluster3x3i");
    define_cluster_finder_bindings<double, 3, 3, uint16_t>(m, "Cluster3x3d");
    define_cluster_finder_bindings<float, 3, 3, uint16_t>(m, "Cluster3x3f");
    define_cluster_finder_bindings<int, 2, 2, uint16_t>(m, "Cluster2x2i");
    define_cluster_finder_bindings<double, 2, 2, uint16_t>(m, "Cluster2x2d");
    define_cluster_finder_bindings<float, 2, 2, uint16_t>(m, "Cluster2x2f");

    define_cluster_finder_mt_bindings<int, 3, 3, uint16_t>(m, "Cluster3x3i");
    define_cluster_finder_mt_bindings<double, 3, 3, uint16_t>(m, "Cluster3x3d");
    define_cluster_finder_mt_bindings<float, 3, 3, uint16_t>(m, "Cluster3x3f");
    define_cluster_finder_mt_bindings<int, 2, 2, uint16_t>(m, "Cluster2x2i");
    define_cluster_finder_mt_bindings<double, 2, 2, uint16_t>(m, "Cluster2x2d");
    define_cluster_finder_mt_bindings<float, 2, 2, uint16_t>(m, "Cluster2x2f");

    define_cluster_file_sink_bindings<int, 3, 3, uint16_t>(m, "Cluster3x3i");
    define_cluster_file_sink_bindings<double, 3, 3, uint16_t>(m, "Cluster3x3d");
    define_cluster_file_sink_bindings<float, 3, 3, uint16_t>(m, "Cluster3x3f");
    define_cluster_file_sink_bindings<int, 2, 2, uint16_t>(m, "Cluster2x2i");
    define_cluster_file_sink_bindings<double, 2, 2, uint16_t>(m, "Cluster2x2d");
    define_cluster_file_sink_bindings<float, 2, 2, uint16_t>(m, "Cluster2x2f");

    define_cluster_collector_bindings<int, 3, 3, uint16_t>(m, "Cluster3x3i");
    define_cluster_collector_bindings<double, 3, 3, uint16_t>(m, "Cluster3x3f");
    define_cluster_collector_bindings<float, 3, 3, uint16_t>(m, "Cluster3x3d");
    define_cluster_collector_bindings<int, 2, 2, uint16_t>(m, "Cluster2x2i");
    define_cluster_collector_bindings<double, 2, 2, uint16_t>(m, "Cluster2x2f");
    define_cluster_collector_bindings<float, 2, 2, uint16_t>(m, "Cluster2x2d");

    define_cluster<int, 3, 3, uint16_t>(m, "3x3i");
    define_cluster<float, 3, 3, uint16_t>(m, "3x3f");
    define_cluster<double, 3, 3, uint16_t>(m, "3x3d");
    define_cluster<int, 2, 2, uint16_t>(m, "2x2i");
    define_cluster<float, 2, 2, uint16_t>(m, "2x2f");
    define_cluster<double, 2, 2, uint16_t>(m, "2x2d");

    register_calculate_eta<int, 3, 3, uint16_t>(m);
    register_calculate_eta<float, 3, 3, uint16_t>(m);
    register_calculate_eta<double, 3, 3, uint16_t>(m);
    register_calculate_eta<int, 2, 2, uint16_t>(m);
    register_calculate_eta<float, 2, 2, uint16_t>(m);
    register_calculate_eta<double, 2, 2, uint16_t>(m);
}
