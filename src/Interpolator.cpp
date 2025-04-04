#include "aare/Interpolator.hpp"

namespace aare {

Interpolator::Interpolator(NDView<double, 3> etacube, NDView<double, 1> xbins,
                           NDView<double, 1> ybins, NDView<double, 1> ebins)
    : m_ietax(etacube), m_ietay(etacube), m_etabinsx(xbins), m_etabinsy(ybins),
      m_energy_bins(ebins) {
    if (etacube.shape(0) != xbins.size() || etacube.shape(1) != ybins.size() ||
        etacube.shape(2) != ebins.size()) {
        throw std::invalid_argument(
            "The shape of the etacube does not match the shape of the bins");
    }

    // Cumulative sum in the x direction
    for (ssize_t i = 1; i < m_ietax.shape(0); i++) {
        for (ssize_t j = 0; j < m_ietax.shape(1); j++) {
            for (ssize_t k = 0; k < m_ietax.shape(2); k++) {
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

    // Cumulative sum in the y direction
    for (ssize_t i = 0; i < m_ietay.shape(0); i++) {
        for (ssize_t j = 1; j < m_ietay.shape(1); j++) {
            for (ssize_t k = 0; k < m_ietay.shape(2); k++) {
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

} // namespace aare