// Files with bindings to the different classes

// New style file naming
#include "bind_Cluster.hpp"
#include "bind_ClusterCollector.hpp"
#include "bind_ClusterFile.hpp"
#include "bind_ClusterFileSink.hpp"
#include "bind_ClusterFinder.hpp"
#include "bind_ClusterFinderMT.hpp"
#include "bind_ClusterVector.hpp"
#include "bind_calibration.hpp"

// TODO! migrate the other names
#include "ctb_raw_file.hpp"
#include "file.hpp"
#include "fit.hpp"
#include "interpolation.hpp"
#include "jungfrau_data_file.hpp"
#include "pedestal.hpp"
#include "pixel_map.hpp"
#include "raw_file.hpp"
#include "raw_master_file.hpp"
#include "raw_sub_file.hpp"
#include "var_cluster.hpp"

// Pybind stuff
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

/* MACRO that defines Cluster bindings for a specific size and type

T - Storage type of the cluster data (int, float, double)
N - Number of rows in the cluster
M - Number of columns in the cluster
U - Type of the pixel data (e.g., uint16_t)
TYPE_CODE - A character representing the type code (e.g., 'i' for int, 'd' for
double, 'f' for float)

*/
#define DEFINE_CLUSTER_BINDINGS(T, N, M, U, TYPE_CODE)                         \
    define_ClusterFile<T, N, M, U>(m, "Cluster" #N "x" #M #TYPE_CODE);         \
    define_ClusterVector<T, N, M, U>(m, "Cluster" #N "x" #M #TYPE_CODE);       \
    define_ClusterFinder<T, N, M, U>(m, "Cluster" #N "x" #M #TYPE_CODE);       \
    define_ClusterFinderMT<T, N, M, U>(m, "Cluster" #N "x" #M #TYPE_CODE);     \
    define_ClusterFileSink<T, N, M, U>(m, "Cluster" #N "x" #M #TYPE_CODE);     \
    define_ClusterCollector<T, N, M, U>(m, "Cluster" #N "x" #M #TYPE_CODE);    \
    define_Cluster<T, N, M, U>(m, #N "x" #M #TYPE_CODE);                       \
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

    bind_calibration(m);

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
}
