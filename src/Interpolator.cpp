#include "aare/Interpolator.hpp"

namespace aare {

Interpolator::Interpolator(NDView<double, 1> xbins, NDView<double, 1> ybins,
                           NDView<double, 1> ebins)
    : m_etabinsx(xbins), m_etabinsy(ybins), m_energy_bins(ebins){};

Interpolator::Interpolator(NDView<double, 3> etacube, NDView<double, 1> xbins,
                           NDView<double, 1> ybins, NDView<double, 1> ebins)
    : m_ietax(etacube), m_ietay(etacube), m_etabinsx(xbins), m_etabinsy(ybins),
      m_energy_bins(ebins) {
    if (etacube.shape(0) + 1 != xbins.size() ||
        etacube.shape(1) + 1 != ybins.size() ||
        etacube.shape(2) + 1 != ebins.size()) {
        throw std::invalid_argument(
            "The shape of the etacube does not match the shape of the bins");
    }
    m_ietax = NDArray<double, 3>(
        std::array<ssize_t, 3>{xbins.size(), ybins.size(), ebins.size()});
    m_ietax(0, 0, 0) = 0.0;

    // Cumulative sum in the x direction
    for (ssize_t i = 1; i < m_ietax.shape(0); i++) {
        for (ssize_t j = 0; j < m_ietax.shape(1); j++) {
            for (ssize_t k = 0; k < m_ietax.shape(2); k++) {
                m_ietax(i, j, k) = etacube(i - 1, j, k);
                m_ietax(i, j, k) += m_ietax(i - 1, j, k);
            }
        }
    }

    // Normalize by the highest row, if norm less than 1 don't do anything
    for (ssize_t i = 0; i < m_ietax.shape(0); i++) {
        for (ssize_t j = 0; j < m_ietax.shape(1); j++) {
            for (ssize_t k = 0; k < m_ietax.shape(2); k++) {
                auto val = m_ietax(m_ietax.shape(0) - 1, j, k);
                double norm = val < 1 ? 1 : val;
                m_ietax(i, j, k) /= norm;
            }
        }
    }

    m_ietay = NDArray<double, 3>(
        std::array<ssize_t, 3>{xbins.size(), ybins.size(), ebins.size()});
    m_ietay(0, 0, 0) = 0.0;

    // Cumulative sum in the y direction
    for (ssize_t i = 0; i < m_ietay.shape(0); i++) {
        for (ssize_t j = 1; j < m_ietay.shape(1); j++) {
            for (ssize_t k = 0; k < m_ietay.shape(2); k++) {
                m_ietax(i, j, k) = etacube(i - 1, j, k);
                m_ietay(i, j, k) += m_ietay(i, j - 1, k);
            }
        }
    }

    // Normalize by the highest column, if norm less than 1 don't do anything
    for (ssize_t i = 0; i < m_ietay.shape(0); i++) {
        for (ssize_t j = 0; j < m_ietay.shape(1); j++) {
            for (ssize_t k = 0; k < m_ietay.shape(2); k++) {
                auto val = m_ietay(i, m_ietay.shape(1) - 1, k);
                double norm = val < 1 ? 1 : val;
                m_ietay(i, j, k) /= norm;
            }
        }
    }
}

void Interpolator::rosenblatttransform(NDView<double, 3> etacube) {

    if (etacube.shape(0) + 1 != m_etabinsx.size() ||
        etacube.shape(1) + 1 != m_etabinsy.size() ||
        etacube.shape(2) != m_energy_bins.size()) {
        throw std::invalid_argument(
            "The shape of the etacube does not match the shape of the bins");
    }

    // TODO: less loops and better performance if ebins is first dimension
    // (violates backwardscompatibility ieta_x and ieta_y public getters,
    // previously generated etacubes)
    //  TODO: maybe more loops is better then storing total_sum_y and
    //  total_sum_x

    // marginal CDF for eta_x
    NDArray<double, 2> marg_CDF_EtaX(
        std::array<ssize_t, 2>{m_etabinsx.size(), m_energy_bins.size()},
        0.0); // +1 simulate proper probability distribution with zero at start

    // conditional CDF for eta_y
    NDArray<double, 3> cond_CDF_EtaY(
        std::array<ssize_t, 3>{m_etabinsx.size(), m_etabinsy.size(),
                               m_energy_bins.size()},
        0.0); // +1 simulate proper probability distribution with zero at start

    // TODO: after changing loop orders check if faster to iterate twice during
    // normalization than storing total sums
    NDArray<double, 1> total_sum_etax(
        std::array<ssize_t, 1>{m_energy_bins.size()},
        0.0); // for nomalization of etax

    NDArray<double, 2> total_sum_etay(
        std::array<ssize_t, 2>{m_etabinsx.size(), m_energy_bins.size()},
        0.0); // for normalization of etay

    for (ssize_t i = 1; i < m_etabinsx.size(); ++i) {
        for (ssize_t j = 1; j < m_etabinsy.size(); ++j) {
            for (ssize_t k = 0; k < m_energy_bins.size(); ++k) {
                marg_CDF_EtaX(i, k) +=
                    etacube(i - 1, j - 1, k); // marginal probability for etaX
                total_sum_etax(k) += etacube(i - 1, j - 1, k);
                cond_CDF_EtaY(i, j, k) += etacube(
                    i - 1, j - 1, k); // joint probability for etaY and etaX
                total_sum_etay(i, k) += etacube(i - 1, j - 1, k);
            }
        }
    }

    // calculate marginal CDF for etaX
    for (ssize_t i = 1; i < m_etabinsx.size(); ++i) {
        for (ssize_t k = 0; k < m_energy_bins.size(); ++k) {
            double norm = total_sum_etax(k) < 1 ? 1 : total_sum_etax(k);
            marg_CDF_EtaX(i, k) /=
                norm; // first element is zero no need to normalize
            marg_CDF_EtaX(i, k) += marg_CDF_EtaX(i - 1, k);
        }
    }

    // calculate conditional CDF for etaY
    // Note P(EtaY|EtaX) = P(EtaY,EtaX)/P(EtaX) we dont divide by P(EtaX) as it
    // cancels out during normalization
    for (ssize_t i = 1; i < m_etabinsx.size(); ++i) {
        for (ssize_t j = 1; j < m_etabinsy.size(); ++j) {
            for (ssize_t k = 0; k < m_energy_bins.size(); ++k) {
                // if smaller than 1 keep zero conditional probability undefined
                double norm =
                    total_sum_etay(i, k) < 1 ? 1 : total_sum_etay(i, k);
                cond_CDF_EtaY(i, j, k) /= norm;
                cond_CDF_EtaY(i, j, k) += cond_CDF_EtaY(i, j - 1, k);
            }
        }
    }

    m_ietay = std::move(
        cond_CDF_EtaY); // TODO maybe rename m_ietay to lookup or CDF_EtaY_cond

    // TODO: should actually be only 2dimensional keep three dimension due to
    // consistency with Annas code change though
    m_ietax = NDArray<double, 3>(std::array<ssize_t, 3>{
        m_etabinsx.size(), m_etabinsy.size(), m_energy_bins.size()});

    for (ssize_t i = 0; i < m_etabinsx.size(); ++i)
        for (ssize_t j = 0; j < m_etabinsy.size(); ++j)
            for (ssize_t k = 0; k < m_energy_bins.size(); ++k)
                m_ietax(i, j, k) = marg_CDF_EtaX(i, k);
}

} // namespace aare