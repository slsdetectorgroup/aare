// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "aare/Models.hpp"
#include <array>
#include <stdexcept>
#include <string>
#include <vector>

namespace aare {

/**
 * @brief Fit configuration for one model: parameter limits, fixed
 * parameters, user start values and minimiser settings.
 *
 * The object holds plain data and can be shared read-only between threads.
 * Method bodies live in src/Fit.cpp, which explicitly instantiates the class
 * for every supported model.
 */
template <typename Model> class FitModel {
  public:
    static constexpr std::size_t npar = Model::npar;

    /**
     * @brief Construct a fit model with sensible defaults.
     *
     * @param strategy        Accepted for compatibility with the earlier
     *                        Minuit2 based implementation. Ignored by the
     *                        built-in solver and forwarded to the optional
     *                        Minuit2 validation backend.
     * @param max_calls       Maximum number of model evaluations per pixel.
     * @param tolerance       EDM tolerance in Minuit's convention: the fit
     *                        stops when the estimated distance to the
     *                        minimum drops below 0.002 * tolerance.
     * @param compute_errors  If true, return Gauss-Newton parameter errors,
     *                        sqrt(diag((J^T J)^-1)). Fixed parameters and
     *                        parameters ending on a limit report 0.
     */
    explicit FitModel(unsigned int strategy = 0, unsigned int max_calls = 100,
                      double tolerance = 0.5, bool compute_errors = false);

    /**
     * @brief Set lower and upper limits for parameter idx.
     *
     * Infinite values leave that side open. Throws std::runtime_error when
     * lo >= hi and std::out_of_range for a bad index.
     */
    void SetParLimits(unsigned int idx, double lo, double hi);

    /**
     * @brief Fix parameter idx at value val.
     *
     * Excluded from minimisation. Automatic estimates will not touch it.
     */
    void FixParameter(unsigned int idx, double val);

    /** @brief Release a previously fixed parameter, re-enabling auto estimates.
     */
    void ReleaseParameter(unsigned int idx);
    void ReleaseParameter(const std::string &name);

    /** @brief Set an explicit starting value for parameter idx.*/
    void SetParameter(unsigned int idx, double val);
    void SetParameter(const std::string &name, double val);
    void FixParameter(const std::string &name, double val);
    void SetParLimits(const std::string &name, double lo, double hi);
    std::string GetParName(unsigned int idx) const;
    std::vector<std::string> GetParNames() const;

    static constexpr std::size_t GetNpar() noexcept { return npar; }
    void SetMaxCalls(unsigned int n) { max_calls_ = n; }
    void SetTolerance(double t) { tolerance_ = t; }
    void SetComputeErrors(bool b) { compute_errors_ = b; }

    unsigned int strategy() const { return strategy_; }
    unsigned int max_calls() const { return max_calls_; }
    double tolerance() const { return tolerance_; }
    bool compute_errors() const { return compute_errors_; }
    bool is_user_fixed(unsigned int idx) const { return user_fixed_[idx]; }
    bool is_user_start(unsigned int idx) const { return user_start_[idx]; }
    /** @brief Lower limit of parameter idx, -infinity when open. */
    double lower_limit(unsigned int idx) const { return lower_[idx]; }
    /** @brief Upper limit of parameter idx, +infinity when open. */
    double upper_limit(unsigned int idx) const { return upper_[idx]; }
    /** @brief User start or fixed value of parameter idx, 0 when unset. */
    double value(unsigned int idx) const { return value_[idx]; }

  private:
    void check_index(unsigned int idx) const;
    unsigned int checked_index(const std::string &name) const;

    unsigned int strategy_;
    unsigned int max_calls_;
    double tolerance_;
    bool compute_errors_;
    std::array<double, npar> lower_{};
    std::array<double, npar> upper_{};
    std::array<double, npar> value_{};
    std::array<bool, npar> user_fixed_{};
    std::array<bool, npar> user_start_{};
};

// Suppress implicit instantiation for all supported model types.
// Definitions live in src/Fit.cpp.
extern template class FitModel<model::Gaussian>;
extern template class FitModel<model::GaussianErfcPlateau>;
extern template class FitModel<model::GaussianChargeSharing>;
extern template class FitModel<model::GaussianChargeSharingKb>;
extern template class FitModel<model::Pol1>;
extern template class FitModel<model::Pol2>;
extern template class FitModel<model::RisingScurve>;
extern template class FitModel<model::FallingScurve>;

} // namespace aare
