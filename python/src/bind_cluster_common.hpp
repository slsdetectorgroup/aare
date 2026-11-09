// SPDX-License-Identifier: MPL-2.0
// Shared by the per-cluster-size translation units bind_cluster_NxN.cpp.
#pragma once

#include "module_config.hpp"

#include "bind_Cluster.hpp"
#include "bind_ClusterCollector.hpp"
#include "bind_ClusterFile.hpp"
#include "bind_ClusterFileSink.hpp"
#include "bind_ClusterFinder.hpp"
#include "bind_ClusterFinderMT.hpp"
#include "bind_ClusterVector.hpp"
#include "bind_Eta.hpp"

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
    define_Cluster<T, N, M, U>(m, #N "x" #M #TYPE_CODE);                       \
    register_calculate_2x2eta<T, N, M, U>(m);                                  \
    define_2x2_reduction<T, N, M, U>(m);                                       \
    reduce_to_2x2<T, N, M, U>(m);

#define DEFINE_BINDINGS_CLUSTERFINDER(T, N, M, U, TYPE_CODE)                   \
    define_ClusterFinder<T, N, M, U>(m, "Cluster" #N "x" #M #TYPE_CODE);       \
    define_ClusterFinderMT<T, N, M, U>(m, "Cluster" #N "x" #M #TYPE_CODE);     \
    define_ClusterFileSink<T, N, M, U>(m, "Cluster" #N "x" #M #TYPE_CODE);     \
    define_ClusterCollector<T, N, M, U>(m, "Cluster" #N "x" #M #TYPE_CODE);
