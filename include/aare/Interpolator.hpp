#pragma once

#include "aare/Cluster.hpp"
#include "aare/ClusterFile.hpp" //Cluster_3x3
#include "aare/ClusterVector.hpp"
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"

namespace aare {

struct Photon {
    double x;
    double y;
    double energy;
};

class Interpolator {
    NDArray<double, 3> m_ietax;
    NDArray<double, 3> m_ietay;

    NDArray<double, 1> m_etabinsx;
    NDArray<double, 1> m_etabinsy;
    NDArray<double, 1> m_energy_bins;

  public:
    Interpolator(NDView<double, 3> etacube, NDView<double, 1> xbins,
                 NDView<double, 1> ybins, NDView<double, 1> ebins);
    NDArray<double, 3> get_ietax() { return m_ietax; }
    NDArray<double, 3> get_ietay() { return m_ietay; }

    template <typename ClusterType>
    std::vector<Photon> interpolate(const ClusterVector<ClusterType> &clusters);
};

} // namespace aare