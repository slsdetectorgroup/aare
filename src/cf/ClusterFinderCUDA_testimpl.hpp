// SPDX-License-Identifier: MPL-2.0
// Plain C++ interface to the CUDA test work in ClusterFinderCUDA_testimpl.cu,
// so the Catch2 test case itself never has to be compiled by nvcc.
#pragma once
#include "aare/Cluster.hpp"
#include <cstdint>
#include <vector>

namespace aare::test_cuda {

/// Clusters found per frame by two independent routes over the same data:
/// `manual` drives FixedWindow::launch on hand-allocated device buffers,
/// `driver` goes through ClusterFinderCUDA. They must agree exactly.
template <typename C> struct ManualVsDriver {
    std::vector<std::vector<C>> manual;
    std::vector<std::vector<C>> driver;
};

ManualVsDriver<Cluster<int32_t, 3, 3>>
run_fixed_window_3x3(int nrows, int ncols, int n_ped, int n_frames);
ManualVsDriver<Cluster<int32_t, 9, 9>>
run_fixed_window_9x9(int nrows, int ncols, int n_ped, int n_frames);

struct FailedRegistration {
    bool registration_threw; // the second registration of one buffer
    bool find_threw;         // the find on a new finder afterwards
    std::vector<std::vector<Cluster<int32_t, 3, 3>>> before; // clean run
    std::vector<std::vector<Cluster<int32_t, 3, 3>>> after;  // after it
};

/// Find clusters with a new finder, register one buffer in two finders (CUDA
/// refuses the second), then find again with another new finder. A CUDA error
/// left over from the failed call must not make that find throw after its
/// work was queued, and both finds must give the same clusters.
FailedRegistration find_after_failed_registration(int nrows, int ncols,
                                                  int n_ped, int n_frames);

} // namespace aare::test_cuda
