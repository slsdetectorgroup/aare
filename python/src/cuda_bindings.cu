// SPDX-License-Identifier: MPL-2.0
//
// CUDA-only Python extension module. Registers ClusterFinderCUDA along with
// the ClusterVector and Cluster types it exposes in its return values, so
// the module is self-contained — users can call steal_clusters() and get
// back a usable ClusterVector without _aare needing to be imported first.

#include "bind_Cluster.hpp"
#include "bind_ClusterFinderCUDA.hpp"
#include "bind_ClusterVector.hpp"

#include <pybind11/pybind11.h>

namespace py = pybind11;

// Register the Cluster + ClusterVector pair for one (T, N, M) combination.
// Subset of DEFINE_CLUSTER_BINDINGS from module.cpp: we register what
// ClusterFinderCUDA actually returns, nothing more. File I/O, eta and
// reduce_to_2x2 stay on the CPU side.
#define DEFINE_CUDA_CLUSTER_TYPES(T, N, M, U, TYPE_CODE)                       \
    define_ClusterVector<T, N, M, U>(m, "Cluster" #N "x" #M #TYPE_CODE);       \
    define_Cluster<T, N, M, U>(m, #N "x" #M #TYPE_CODE);

#define DEFINE_BINDINGS_CLUSTERFINDER_CUDA(T, N, M, U, TYPE_CODE)              \
    aare::define_ClusterFinderCUDA<T, N, M, U>(m, "Cluster" #N                 \
                                                  "x" #M #TYPE_CODE);

PYBIND11_MODULE(_aare_cuda, m) {

    // Types first — finders reference them in their signatures.
    // SFINAE excludes 2x2 on ClusterFinderCUDA, so we skip it here too.
    DEFINE_CUDA_CLUSTER_TYPES(int, 3, 3, uint16_t, i);
    DEFINE_CUDA_CLUSTER_TYPES(double, 3, 3, uint16_t, d);
    DEFINE_CUDA_CLUSTER_TYPES(float, 3, 3, uint16_t, f);

    DEFINE_CUDA_CLUSTER_TYPES(int, 5, 5, uint16_t, i);
    DEFINE_CUDA_CLUSTER_TYPES(double, 5, 5, uint16_t, d);
    DEFINE_CUDA_CLUSTER_TYPES(float, 5, 5, uint16_t, f);

    DEFINE_CUDA_CLUSTER_TYPES(int, 7, 7, uint16_t, i);
    DEFINE_CUDA_CLUSTER_TYPES(double, 7, 7, uint16_t, d);
    DEFINE_CUDA_CLUSTER_TYPES(float, 7, 7, uint16_t, f);

    DEFINE_CUDA_CLUSTER_TYPES(int, 9, 9, uint16_t, i);
    DEFINE_CUDA_CLUSTER_TYPES(double, 9, 9, uint16_t, d);
    DEFINE_CUDA_CLUSTER_TYPES(float, 9, 9, uint16_t, f);

    // Finders
    DEFINE_BINDINGS_CLUSTERFINDER_CUDA(int, 3, 3, uint16_t, i);
    DEFINE_BINDINGS_CLUSTERFINDER_CUDA(double, 3, 3, uint16_t, d);
    DEFINE_BINDINGS_CLUSTERFINDER_CUDA(float, 3, 3, uint16_t, f);

    DEFINE_BINDINGS_CLUSTERFINDER_CUDA(int, 5, 5, uint16_t, i);
    DEFINE_BINDINGS_CLUSTERFINDER_CUDA(double, 5, 5, uint16_t, d);
    DEFINE_BINDINGS_CLUSTERFINDER_CUDA(float, 5, 5, uint16_t, f);

    DEFINE_BINDINGS_CLUSTERFINDER_CUDA(int, 7, 7, uint16_t, i);
    DEFINE_BINDINGS_CLUSTERFINDER_CUDA(double, 7, 7, uint16_t, d);
    DEFINE_BINDINGS_CLUSTERFINDER_CUDA(float, 7, 7, uint16_t, f);

    DEFINE_BINDINGS_CLUSTERFINDER_CUDA(int, 9, 9, uint16_t, i);
    DEFINE_BINDINGS_CLUSTERFINDER_CUDA(double, 9, 9, uint16_t, d);
    DEFINE_BINDINGS_CLUSTERFINDER_CUDA(float, 9, 9, uint16_t, f);
}

#undef DEFINE_CUDA_CLUSTER_TYPES
#undef DEFINE_BINDINGS_CLUSTERFINDER_CUDA