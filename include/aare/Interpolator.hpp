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

    /**
     * @brief Constructor for the Interpolator class
     * @param xbins bin edges for etaX
     * @param ybins bin edges for etaY
     * @param ebins bin edges for photon energy
     */
    Interpolator(NDView<double, 1> xbins, NDView<double, 1> ybins,
                 NDView<double, 1> ebins);

    /**
     * @brief transforms the joint eta distribution of etaX and etaY to the two
     * independant uniform distributions based on the Roseblatt transform for
     * each energy level
     * @param etacube joint distribution of etaX, etaY and photon energy
     * @note note first dimension is etaX, second etaY, third photon energy
     */
    void rosenblatttransform(NDView<double, 3> etacube);

    NDArray<double, 3> get_ietax() { return m_ietax; }
    NDArray<double, 3> get_ietay() { return m_ietay; }

    /**
     * @brief interpolates the cluster centers for all clusters to a better
     * precision
     * @tparam ClusterType Type of Clusters to interpolate
     * @tparam Etafunction Function object that calculates desired eta default:
     * calculate_eta2x2
     * @warning
     */
    template <typename ClusterType, auto EtaFUnction = calculate_eta2,
              typename Eanble = std::enable_if_t<is_cluster_v<ClusterType>>>
    std::vector<Photon> interpolate(const ClusterVector<ClusterType> &clusters);

  private:
    // TODO: is it better to use a functor? with template specialization if the
    // template argument is actually not used?

    /**
     * @brief implements underlying interpolation logic based on EtaFunction
     * Type
     * @tparam EtaType Type of Eta
     * @tparam EtaFunction Function object that calculates desired eta default:
     * @param u: transformed photon position in x between [0,1]
     * @param v: transformed photon position in y between [0,1]
     */
    template <typename ClusterType, auto EtaFunction, typename EtaType>
    void interpolation_logic(Photon &photon, const double u, const double v,
                             const EtaType &eta);
};

template <typename ClusterType, auto EtaFunction, typename Enable>
std::vector<Photon>
Interpolator::interpolate(const ClusterVector<ClusterType> &clusters) {
    std::vector<Photon> photons;
    photons.reserve(clusters.size());

    for (const ClusterType &cluster : clusters) {

        auto eta = EtaFunction(cluster);

        Photon photon;
        photon.x = cluster.x;
        photon.y = cluster.y;
        photon.energy = static_cast<decltype(photon.energy)>(eta.sum);

        // Finding the index of the last element that is smaller
        // should work fine as long as we have many bins
        auto ie = last_smaller(m_energy_bins, photon.energy);
        auto ix = last_smaller(m_etabinsx, eta.x);
        auto iy = last_smaller(m_etabinsy, eta.y);

        // bilinear interpolation
        double ietax_interp_left = linear_interpolation(
            {m_etabinsy(iy), m_etabinsy(iy + 1)},
            {m_ietax(ix, iy, ie), m_ietax(ix, iy + 1, ie)}, eta.y);
        double ietax_interp_right = linear_interpolation(
            {m_etabinsy(iy), m_etabinsy(iy + 1)},
            {m_ietax(ix + 1, iy, ie), m_ietax(ix + 1, iy + 1, ie)}, eta.y);

        // transformed photon position x between [0,1]
        double ietax_interpolated = linear_interpolation(
            {m_etabinsx(ix), m_etabinsx(ix + 1)},
            {ietax_interp_left, ietax_interp_right}, eta.x);

        double ietay_interp_left = linear_interpolation(
            {m_etabinsx(ix), m_etabinsx(ix + 1)},
            {m_ietay(ix, iy, ie), m_ietay(ix + 1, iy, ie)}, eta.x);
        double ietay_interp_right = linear_interpolation(
            {m_etabinsx(ix), m_etabinsx(ix + 1)},
            {m_ietax(ix, iy + 1, ie), m_ietax(ix, iy + 1, ie)}, eta.x);

        // transformed photon position y between [0,1]
        double ietay_interpolated = linear_interpolation(
            {m_etabinsy(iy), m_etabinsy(iy + 1)},
            {ietay_interp_left, ietay_interp_right}, eta.y);

        interpolation_logic<ClusterType, EtaFunction>(
            photon, ietax_interpolated, ietay_interpolated, eta);

        photons.push_back(photon);
    }

    return photons;
}

template <typename ClusterType, auto EtaFunction, typename EtaType>
void Interpolator::interpolation_logic(Photon &photon, const double u,
                                       const double v, const EtaType &eta) {

    // try to call this with std::is_same_v and have it constexpr if possible
    if (EtaFunction ==
        &calculate_eta2<
            typename ClusterType::value_type, ClusterType::cluster_size_x,
            ClusterType::cluster_size_y, typename ClusterType::coord_type>) {
        double dX, dY;

        // TODO: could also chaneg the sign of the eta calculation
        switch (static_cast<corner>(eta.c)) {
        case corner::cTopLeft:
            dX = 0.0;
            dY = 0.0;
            break;
        case corner::cTopRight:;
            dX = 1.0;
            dY = 0.0;
            break;
        case corner::cBottomLeft:
            dX = 0.0;
            dY = 1.0;
            break;
        case corner::cBottomRight:
            dX = 1.0;
            dY = 1.0;
            break;
        }
        photon.x -= u - dX;
        photon.y -= v - dY;
    } else {
        photon.x += u;
        photon.y += v;
    }
}

} // namespace aare