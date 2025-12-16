// SPDX-License-Identifier: MPL-2.0
#include "aare/Interpolator.hpp"

namespace aare {

Interpolator::Interpolator(NDView<double, 1> xbins, NDView<double, 1> ybins,
                           NDView<double, 1> ebins)
    : m_etabinsx(xbins), m_etabinsy(ybins), m_energy_bins(ebins){};

Interpolator::Interpolator(NDView<double, 3> etacube, NDView<double, 1> xbins,
                           NDView<double, 1> ybins, NDView<double, 1> ebins)
    : m_etabinsx(xbins), m_etabinsy(ybins), m_energy_bins(ebins) {
    if (etacube.shape(0) + 1 != xbins.size() ||
        etacube.shape(1) + 1 != ybins.size() ||
        etacube.shape(2) + 1 != ebins.size()) {
        throw std::invalid_argument(
            "The shape of the etacube does not match the shape of the bins");
    }

    m_ietax = NDArray<double, 3>(etacube);

    m_ietay = NDArray<double, 3>(etacube);
    // TODO: etacube should have different strides energy should come first
    //  prefix sum - conditional CDF
    for (ssize_t i = 0; i < m_ietax.shape(0); i++) {
        for (ssize_t j = 0; j < m_ietax.shape(1); j++) {
            for (ssize_t k = 0; k < m_ietax.shape(2); k++) {
                m_ietax(i, j, k) += (i == 0) ? 0 : m_ietax(i - 1, j, k);

                m_ietay(i, j, k) += (j == 0) ? 0 : m_ietay(i, j - 1, k);
            }
        }
    }

    // Standardize, if norm less than 1 don't do anything
    for (ssize_t i = 0; i < m_ietax.shape(0); i++) {
        for (ssize_t j = 0; j < m_ietax.shape(1); j++) {
            for (ssize_t k = 0; k < m_ietax.shape(2); k++) {
                auto shift_x = etacube(0, j, k);
                auto val_etax = m_ietax(m_ietax.shape(0) - 1, j, k) - shift_x;
                double norm_etax = val_etax == 0 ? 1 : val_etax;
                m_ietax(i, j, k) -= shift_x;
                m_ietax(i, j, k) /= norm_etax;
                auto shift_y = etacube(i, 0, k);
                auto val_etay = m_ietay(i, m_ietay.shape(1) - 1, k) - shift_y;
                double norm_etay = val_etay == 0 ? 1 : val_etay;
                m_ietay(i, j, k) -= shift_y;

                m_ietay(i, j, k) /= norm_etay;
            }
        }
    }
}

void Interpolator::rosenblatttransform(NDView<double, 3> etacube) {

    if (etacube.shape(0) + 1 != m_etabinsx.size() ||
        etacube.shape(1) + 1 != m_etabinsy.size() ||
        etacube.shape(2) + 1 != m_energy_bins.size()) {
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
        std::array<ssize_t, 2>{m_etabinsx.size() - 1, m_energy_bins.size() - 1},
        0.0); // simulate proper probability distribution with zero at start

    // conditional CDF for eta_y
    NDArray<double, 3> cond_CDF_EtaY(etacube);

    for (ssize_t i = 0; i < cond_CDF_EtaY.shape(0); ++i) {
        for (ssize_t j = 0; j < cond_CDF_EtaY.shape(1); ++j) {
            for (ssize_t k = 0; k < cond_CDF_EtaY.shape(2); ++k) {
                // cumsum along y-axis
                marg_CDF_EtaX(i, k) +=
                    etacube(i, j,
                            k); // marginal probability for etaX

                // cumsum along y-axis
                cond_CDF_EtaY(i, j, k) +=
                    (j == 0) ? 0 : cond_CDF_EtaY(i, j - 1, k);
            }
        }
    }

    // cumsum along x-axis
    for (ssize_t i = 1; i < marg_CDF_EtaX.shape(0); ++i) {
        for (ssize_t k = 0; k < marg_CDF_EtaX.shape(1); ++k) {
            marg_CDF_EtaX(0, k) =
                0.0; // shift by first value to ensure values between 0 and 1

            marg_CDF_EtaX(i, k) += marg_CDF_EtaX(i - 1, k);
        }
    }

    // normalize marg_CDF_EtaX
    for (ssize_t i = 1; i < marg_CDF_EtaX.shape(0); ++i) {
        for (ssize_t k = 0; k < marg_CDF_EtaX.shape(1); ++k) {
            double norm = marg_CDF_EtaX(marg_CDF_EtaX.shape(0) - 1, k) == 0
                              ? 1
                              : marg_CDF_EtaX(marg_CDF_EtaX.shape(0) - 1, k);
            marg_CDF_EtaX(i, k) /= norm;
        }
    }

    // standardize, normalize conditional CDF for etaY
    // Note P(EtaY|EtaX) = P(EtaY,EtaX)/P(EtaX) we dont divide by P(EtaX) as it
    // cancels out during normalization
    for (ssize_t i = 0; i < cond_CDF_EtaY.shape(0); ++i) {
        for (ssize_t j = 0; j < cond_CDF_EtaY.shape(1); ++j) {
            for (ssize_t k = 0; k < cond_CDF_EtaY.shape(2); ++k) {
                double shift = etacube(i, 0, k);
                double norm =
                    (cond_CDF_EtaY(i, cond_CDF_EtaY.shape(1) - 1, k) - shift) ==
                            0
                        ? 1
                        : cond_CDF_EtaY(i, cond_CDF_EtaY.shape(1) - 1, k) -
                              shift;
                cond_CDF_EtaY(i, j, k) -= shift;
                cond_CDF_EtaY(i, j, k) /= norm;
            }
        }
    }

    m_ietay = std::move(
        cond_CDF_EtaY); // TODO maybe rename m_ietay to lookup or CDF_EtaY_cond

    // TODO: should actually be only 2dimensional keep three dimension due to
    // consistency with Annas code change though
    m_ietax = NDArray<double, 3>(
        std::array<ssize_t, 3>{m_etabinsx.size() - 1, m_etabinsy.size() - 1,
                               m_energy_bins.size() - 1});

    for (ssize_t i = 0; i < m_etabinsx.size() - 1; ++i)
        for (ssize_t j = 0; j < m_etabinsy.size() - 1; ++j)
            for (ssize_t k = 0; k < m_energy_bins.size() - 1; ++k)
                m_ietax(i, j, k) = marg_CDF_EtaX(i, k);
}

} // namespace aare