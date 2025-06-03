// Files with bindings to the different classes

//New style file naming
#include "bind_Cluster.hpp"
#include "bind_ClusterCollector.hpp"
#include "bind_ClusterFinder.hpp"
#include "bind_ClusterFinderMT.hpp"
#include "bind_ClusterFile.hpp"
#include "bind_ClusterFileSink.hpp"
#include "bind_ClusterVector.hpp"

//TODO! migrate the other names
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

/* MACRO that defines Cluster bindings for a specific size and type

T - Storage type of the cluster data (int, float, double)
N - Number of rows in the cluster
M - Number of columns in the cluster
U - Type of the pixel data (e.g., uint16_t)
TYPE_CODE - A character representing the type code (e.g., 'i' for int, 'd' for double, 'f' for float)

*/
#define DEFINE_CLUSTER_BINDINGS(T, N, M, U, TYPE_CODE) \
    define_ClusterFile<T, N, M, U>(m, "Cluster" #N "x" #M #TYPE_CODE); \
    define_ClusterVector<T, N, M, U>(m, "Cluster" #N "x" #M #TYPE_CODE); \
    define_ClusterFinder<T, N, M, U>(m, "Cluster" #N "x" #M #TYPE_CODE); \
    define_ClusterFinderMT<T, N, M, U>(m, "Cluster" #N "x" #M #TYPE_CODE); \
    define_ClusterFileSink<T, N, M, U>(m, "Cluster" #N "x" #M #TYPE_CODE); \
    define_ClusterCollector<T, N, M, U>(m, "Cluster" #N "x" #M #TYPE_CODE); \
    define_Cluster<T, N, M, U>(m, #N "x" #M #TYPE_CODE); \
    register_calculate_eta<T, N, M, U>(m);

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

    DEFINE_CLUSTER_BINDINGS(int, 3, 3, uint16_t, i);
    DEFINE_CLUSTER_BINDINGS(double, 3, 3, uint16_t, d);
    DEFINE_CLUSTER_BINDINGS(float, 3, 3, uint16_t, f);

    DEFINE_CLUSTER_BINDINGS(int, 2, 2, uint16_t, i);
    DEFINE_CLUSTER_BINDINGS(double, 2, 2, uint16_t, d);
    DEFINE_CLUSTER_BINDINGS(float, 2, 2, uint16_t, f);

    DEFINE_CLUSTER_BINDINGS(int, 5, 5, uint16_t, i);
    DEFINE_CLUSTER_BINDINGS(double, 5, 5, uint16_t, d);
    DEFINE_CLUSTER_BINDINGS(float, 5, 5, uint16_t, f);

    DEFINE_CLUSTER_BINDINGS(int, 7, 7, uint16_t, i);
    DEFINE_CLUSTER_BINDINGS(double, 7, 7, uint16_t, d);
    DEFINE_CLUSTER_BINDINGS(float, 7, 7, uint16_t, f);

    DEFINE_CLUSTER_BINDINGS(int, 9, 9, uint16_t, i);
    DEFINE_CLUSTER_BINDINGS(double, 9, 9, uint16_t, d);
    DEFINE_CLUSTER_BINDINGS(float, 9, 9, uint16_t, f);

    // define_ClusterFile<int, 3, 3, uint16_t>(m, "Cluster3x3i");
    // define_ClusterFile<double, 3, 3, uint16_t>(m, "Cluster3x3d");
    // define_ClusterFile<float, 3, 3, uint16_t>(m, "Cluster3x3f");
    // define_ClusterFile<int, 2, 2, uint16_t>(m, "Cluster2x2i");
    // define_ClusterFile<float, 2, 2, uint16_t>(m, "Cluster2x2f");
    // define_ClusterFile<double, 2, 2, uint16_t>(m, "Cluster2x2d");


    // define_ClusterVector<int, 3, 3, uint16_t>(m, "Cluster3x3i");
    // define_ClusterVector<double, 3, 3, uint16_t>(m, "Cluster3x3d");
    // define_ClusterVector<float, 3, 3, uint16_t>(m, "Cluster3x3f");
    // define_ClusterVector<int, 2, 2, uint16_t>(m, "Cluster2x2i");
    // define_ClusterVector<double, 2, 2, uint16_t>(m, "Cluster2x2d");
    // define_ClusterVector<float, 2, 2, uint16_t>(m, "Cluster2x2f");

    // define_ClusterFinder<int, 3, 3, uint16_t>(m, "Cluster3x3i");
    // define_ClusterFinder<double, 3, 3, uint16_t>(m, "Cluster3x3d");
    // define_ClusterFinder<float, 3, 3, uint16_t>(m, "Cluster3x3f");
    // define_ClusterFinder<int, 2, 2, uint16_t>(m, "Cluster2x2i");
    // define_ClusterFinder<double, 2, 2, uint16_t>(m, "Cluster2x2d");
    // define_ClusterFinder<float, 2, 2, uint16_t>(m, "Cluster2x2f");

    // define_ClusterFinderMT<int, 3, 3, uint16_t>(m, "Cluster3x3i");
    // define_ClusterFinderMT<double, 3, 3, uint16_t>(m, "Cluster3x3d");
    // define_ClusterFinderMT<float, 3, 3, uint16_t>(m, "Cluster3x3f");
    // define_ClusterFinderMT<int, 2, 2, uint16_t>(m, "Cluster2x2i");
    // define_ClusterFinderMT<double, 2, 2, uint16_t>(m, "Cluster2x2d");
    // define_ClusterFinderMT<float, 2, 2, uint16_t>(m, "Cluster2x2f");

    // define_ClusterFileSink<int, 3, 3, uint16_t>(m, "Cluster3x3i");
    // define_ClusterFileSink<double, 3, 3, uint16_t>(m, "Cluster3x3d");
    // define_ClusterFileSink<float, 3, 3, uint16_t>(m, "Cluster3x3f");
    // define_ClusterFileSink<int, 2, 2, uint16_t>(m, "Cluster2x2i");
    // define_ClusterFileSink<double, 2, 2, uint16_t>(m, "Cluster2x2d");
    // define_ClusterFileSink<float, 2, 2, uint16_t>(m, "Cluster2x2f");

    // define_ClusterCollector<int, 3, 3, uint16_t>(m, "Cluster3x3i");
    // define_ClusterCollector<double, 3, 3, uint16_t>(m, "Cluster3x3d");
    // define_ClusterCollector<float, 3, 3, uint16_t>(m, "Cluster3x3f");
    // define_ClusterCollector<int, 2, 2, uint16_t>(m, "Cluster2x2i");
    // define_ClusterCollector<double, 2, 2, uint16_t>(m, "Cluster2x2d");
    // define_ClusterCollector<float, 2, 2, uint16_t>(m, "Cluster2x2f");

    // // define_Cluster<int, 3, 3, uint16_t>(m, "3x3i");
    // define_Cluster<float, 3, 3, uint16_t>(m, "3x3f");
    // define_Cluster<double, 3, 3, uint16_t>(m, "3x3d");
    // define_Cluster<int, 2, 2, uint16_t>(m, "2x2i");
    // define_Cluster<float, 2, 2, uint16_t>(m, "2x2f");
    // define_Cluster<double, 2, 2, uint16_t>(m, "2x2d");

    // // register_calculate_eta<int, 3, 3, uint16_t>(m);
    // register_calculate_eta<float, 3, 3, uint16_t>(m);
    // register_calculate_eta<double, 3, 3, uint16_t>(m);
    // register_calculate_eta<int, 2, 2, uint16_t>(m);
    // register_calculate_eta<float, 2, 2, uint16_t>(m);
    // register_calculate_eta<double, 2, 2, uint16_t>(m);
}
