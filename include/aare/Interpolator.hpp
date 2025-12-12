// SPDX-License-Identifier: MPL-2.0
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

struct Coordinate2D {
    double x{};
    double y{};
};

class Interpolator {

    /**
     * @brief
     * marginal CDF of eta_x (if rosenblatt applied), conditional
     * CDF of eta_x conditioned on eta_y
     * value at (i, j, e): F(eta_x[i] |
     *eta_y[j], energy[e])
     */
    NDArray<double, 3> m_ietax;

    /**
     * @brief
     * conditional CDF of eta_y conditioned on eta_x
     * value at (i,j,e): F(eta_y[j] | eta_x[i], energy[e])
     */
    NDArray<double, 3> m_ietay;

    NDArray<double, 1> m_etabinsx;
    NDArray<double, 1> m_etabinsy;
    NDArray<double, 1> m_energy_bins;

  public:
    /**
     * @brief Constructor for the Interpolator class
     * @param etacube joint distribution of etaX, etaY and photon energy (note
     * first dimension is etaX, second etaY, third photon energy)
     * @param xbins bin edges for etaX
     * @param ybins bin edges for etaY
     * @param ebins bin edges for photon energy
     */
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
     * @param etacube joint distribution of etaX, etaY and photon energy (first
     * dimension is etaX, second etaY, third photon energy)
     */
    void rosenblatttransform(NDView<double, 3> etacube);

    NDArray<double, 3> get_ietax() { return m_ietax; }
    NDArray<double, 3> get_ietay() { return m_ietay; }

    /**
     * @brief interpolates the cluster centers for all clusters to a better
     * precision
     * @tparam ClusterType Type of Clusters to interpolate
     * @tparam Etafunction Function object that calculates desired eta default:
     * calculate_eta2
     * @return interpolated photons (photon positions are given as double but
     * following row column format e.g. x=0, y=0 means top row and first column
     * of frame) (An interpolated photon position of (1.5, 2.5) corresponds to
     * an estimated photon hit at the pixel center of pixel (1,2))
     */
    template <auto EtaFunction = calculate_eta2, typename ClusterType,
              typename Eanble = std::enable_if_t<is_cluster_v<ClusterType>>>
    std::vector<Photon>
    interpolate(const ClusterVector<ClusterType> &clusters) const;

    /**
     * @brief transforms the eta values to uniform coordinates based on the CDF
     * ieta_x and ieta_y
     * @tparam eta Eta to transform
     * @return uniform coordinates {x,y}
     */
    template <typename T>
    Coordinate2D transform_eta_values(const Eta2<T> &eta) const;

  private:
    /**
     * @brief bilinear interpolation of the transformed eta values
     * @param ix index of etaX bin
     * @param iy index of etaY bin
     * @param ie index of energy bin
     * @return pair of interpolated transformed eta values (ietax, ietay)
     */
    template <typename T>
    std::pair<double, double>
    bilinear_interpolation(const size_t ix, const size_t iy, const size_t ie,
                           const Eta2<T> &eta) const;
};

template <typename T>
std::pair<double, double>
Interpolator::bilinear_interpolation(const size_t ix, const size_t iy,
                                     const size_t ie,
                                     const Eta2<T> &eta) const {
    auto next_index_y = static_cast<ssize_t>(iy + 1) >= m_ietax.shape(1)
                            ? m_ietax.shape(1) - 1
                            : iy + 1;
    auto next_index_x = static_cast<ssize_t>(ix + 1) >= m_ietax.shape(0)
                            ? m_ietax.shape(0) - 1
                            : ix + 1;

    // bilinear interpolation
    double ietax_interp_left = linear_interpolation(
        {m_etabinsy(iy), m_etabinsy(iy + 1)},
        {m_ietax(ix, iy, ie), m_ietax(ix, next_index_y, ie)}, eta.y);
    double ietax_interp_right =
        linear_interpolation({m_etabinsy(iy), m_etabinsy(iy + 1)},
                             {m_ietax(next_index_x, iy, ie),
                              m_ietax(next_index_x, next_index_y, ie)},
                             eta.y);

    // transformed photon position x between [0,1]
    double ietax_interpolated =
        linear_interpolation({m_etabinsx(ix), m_etabinsx(ix + 1)},
                             {ietax_interp_left, ietax_interp_right}, eta.x);

    double ietay_interp_left = linear_interpolation(
        {m_etabinsx(ix), m_etabinsx(ix + 1)},
        {m_ietay(ix, iy, ie), m_ietay(next_index_x, iy, ie)}, eta.x);
    double ietay_interp_right =
        linear_interpolation({m_etabinsx(ix), m_etabinsx(ix + 1)},
                             {m_ietay(ix, next_index_y, ie),
                              m_ietay(next_index_x, next_index_y, ie)},
                             eta.x);

    // transformed photon position y between [0,1]
    double ietay_interpolated =
        linear_interpolation({m_etabinsy(iy), m_etabinsy(iy + 1)},
                             {ietay_interp_left, ietay_interp_right}, eta.y);

    return {ietax_interpolated, ietay_interpolated};
}

template <typename T>
Coordinate2D Interpolator::transform_eta_values(const Eta2<T> &eta) const {

    // Finding the index of the last element that is smaller
    // should work fine as long as we have many bins
    auto ie = last_smaller(m_energy_bins, static_cast<double>(eta.sum));
    auto ix = last_smaller(m_etabinsx, eta.x);
    auto iy = last_smaller(m_etabinsy, eta.y);

    // TODO: bilinear interpolation only works if all bins have a size > 1 -
    // otherwise bilinear interpolation with zero values which skew the
    // results
    // TODO: maybe trim the bins at the edges with zero values beforehand
    // auto [ietax_interpolated, ietay_interpolated] =
    // bilinear_interpolation(ix, iy, ie, eta);

    return Coordinate2D{m_ietax(ix, iy, ie), m_ietay(ix, iy, ie)};
}

template <auto EtaFunction, typename ClusterType, typename Enable>
std::vector<Photon>
Interpolator::interpolate(const ClusterVector<ClusterType> &clusters) const {
    std::vector<Photon> photons;
    photons.reserve(clusters.size());

    for (const ClusterType &cluster : clusters) {

        auto eta = EtaFunction(cluster);

        Photon photon;
        photon.x = cluster.x;
        photon.y = cluster.y;
        photon.energy = static_cast<decltype(photon.energy)>(eta.sum);

        auto uniform_coordinates = transform_eta_values(eta);

        if (EtaFunction == &calculate_eta2<typename ClusterType::value_type,
                                           ClusterType::cluster_size_x,
                                           ClusterType::cluster_size_y,
                                           typename ClusterType::coord_type> ||
            EtaFunction ==
                &calculate_full_eta2<typename ClusterType::value_type,
                                     ClusterType::cluster_size_x,
                                     ClusterType::cluster_size_y,
                                     typename ClusterType::coord_type>) {
            double dX{}, dY{};

            // TODO: could also chaneg the sign of the eta calculation
            switch (eta.c) {
            case corner::cTopLeft:
                dX = -1.0;
                dY = -1.0;
                break;
            case corner::cTopRight:;
                dX = 0.0;
                dY = -1.0;
                break;
            case corner::cBottomLeft:
                dX = -1.0;
                dY = 0.0;
                break;
            case corner::cBottomRight:
                dX = 0.0;
                dY = 0.0;
                break;
            }
            photon.x = photon.x + 0.5 + uniform_coordinates.x +
                       dX; // use pixel center + 0.5
            photon.y =
                photon.y + 0.5 + uniform_coordinates.y +
                dY; // eta2 calculates the ratio between bottom and sum of
                    // bottom and top  shift by 1 add eta value correctly
        } else {
            photon.x += uniform_coordinates.x;
            photon.y += uniform_coordinates.y;
        }

        photons.push_back(photon);
    }

    return photons;
}

} // namespace aare