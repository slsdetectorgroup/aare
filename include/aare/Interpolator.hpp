#pragma once

#include "aare/CalculateEta.hpp"
#include "aare/Cluster.hpp"
#include "aare/ClusterFile.hpp" //Cluster_3x3
#include "aare/ClusterVector.hpp"
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include "aare/algorithm.hpp"

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

    template <typename ClusterType,
              typename Eanble = std::enable_if_t<is_cluster_v<ClusterType>>>
    std::vector<Photon> interpolate(const ClusterVector<ClusterType> &clusters);
};

// TODO: generalize to support any clustertype!!! otherwise add std::enable_if_t
// to only take Cluster2x2 and Cluster3x3
template <typename ClusterType, typename Enable>
std::vector<Photon>
Interpolator::interpolate(const ClusterVector<ClusterType> &clusters) {
    std::vector<Photon> photons;
    photons.reserve(clusters.size());

    if (clusters.cluster_size_x() == 3 || clusters.cluster_size_y() == 3) {
        for (const ClusterType &cluster : clusters) {

            auto eta = calculate_eta2(cluster);

            Photon photon;
            photon.x = cluster.x;
            photon.y = cluster.y;
            photon.energy = static_cast<decltype(photon.energy)>(eta.sum);

            // auto ie = nearest_index(m_energy_bins, photon.energy)-1;
            // auto ix = nearest_index(m_etabinsx, eta.x)-1;
            // auto iy = nearest_index(m_etabinsy, eta.y)-1;
            // Finding the index of the last element that is smaller
            // should work fine as long as we have many bins
            auto ie = last_smaller(m_energy_bins, photon.energy);
            auto ix = last_smaller(m_etabinsx, eta.x);
            auto iy = last_smaller(m_etabinsy, eta.y);

            // fmt::print("ex: {}, ix: {}, iy: {}\n", ie, ix, iy);

            double dX, dY;
            // cBottomLeft = 0,
            // cBottomRight = 1,
            // cTopLeft = 2,
            // cTopRight = 3
            switch (static_cast<corner>(eta.c)) {
            case corner::cTopLeft:
                dX = -1.;
                dY = 0;
                break;
            case corner::cTopRight:;
                dX = 0;
                dY = 0;
                break;
            case corner::cBottomLeft:
                dX = -1.;
                dY = -1.;
                break;
            case corner::cBottomRight:
                dX = 0.;
                dY = -1.;
                break;
            }
            photon.x += m_ietax(ix, iy, ie) * 2 + dX;
            photon.y += m_ietay(ix, iy, ie) * 2 + dY;
            photons.push_back(photon);
        }
    } else if (clusters.cluster_size_x() == 2 ||
               clusters.cluster_size_y() == 2) {
        for (const ClusterType &cluster : clusters) {
            auto eta = calculate_eta2(cluster);

            Photon photon;
            photon.x = cluster.x;
            photon.y = cluster.y;
            photon.energy = static_cast<decltype(photon.energy)>(eta.sum);

            // Now do some actual interpolation.
            // Find which energy bin the cluster is in
            //  auto ie = nearest_index(m_energy_bins, photon.energy)-1;
            //  auto ix = nearest_index(m_etabinsx, eta.x)-1;
            //  auto iy = nearest_index(m_etabinsy, eta.y)-1;
            // Finding the index of the last element that is smaller
            // should work fine as long as we have many bins
            auto ie = last_smaller(m_energy_bins, photon.energy);
            auto ix = last_smaller(m_etabinsx, eta.x);
            auto iy = last_smaller(m_etabinsy, eta.y);

            photon.x += m_ietax(ix, iy, ie) *
                        2; // eta goes between 0 and 1 but we could move the hit
                           // anywhere in the 2x2
            photon.y += m_ietay(ix, iy, ie) * 2;
            photons.push_back(photon);
        }

    } else {
        throw std::runtime_error(
            "Only 3x3 and 2x2 clusters are supported for interpolation");
    }

    return photons;
}

} // namespace aare