// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "aare/Models.hpp"
#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace aare {

template <typename Model> class FitModel {

    // Forward declaration only — full definition lives in src/FitModelImpl.hpp
    // and is never installed. Downstream code cannot dereference this pointer.
    struct FitModelImpl; // stores Minuit2 user-set parameters

    std::unique_ptr<FitModelImpl> impl_;
    unsigned int max_calls_;
    double tolerance_;
    bool compute_errors_;

    std::array<bool, Model::npar> user_fixed_{};
    std::array<bool, Model::npar> user_start_{};

    /** @brief Safely resolve a parameter name to its index. */
    unsigned int checked_index(const std::string &name) const;

  public:
    static constexpr std::size_t npar = Model::npar;

    /**
     * @brief Construct a fit model with sensible defaults.
     *
     * @param strategy        Minuit2 strategy level (0 = fast/gradient, 1 =
     * default).
     * @param max_calls       Maximum FCN calls per pixel minimisation.
     * @param tolerance       Minuit2 EDM tolerance.
     * @param compute_errors  If true, run MnHesse after minimisation.
     */
    explicit FitModel(unsigned int strategy = 0, unsigned int max_calls = 100,
                      double tolerance = 0.5, bool compute_errors = false);

    // Destructor must be defined in Fit.cpp where FitModelImpl is complete.
    ~FitModel();
    FitModel(const FitModel &);
    FitModel &operator=(const FitModel &);
    FitModel(FitModel &&) noexcept = default;
    FitModel &operator=(FitModel &&) noexcept = default;

    /** @brief Set lower and upper bounds for parameter idx.*/
    void SetParLimits(unsigned int idx, double lo, double hi);

    /**
     * @brief Fix parameter idx at value val.
     *
     * Excluded from minimisation.  Automatic estimates will not touch it.
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

    unsigned int max_calls() const { return max_calls_; }
    double tolerance() const { return tolerance_; }
    bool compute_errors() const { return compute_errors_; }
    bool is_user_fixed(unsigned int idx) const { return user_fixed_[idx]; }
    bool is_user_start(unsigned int idx) const { return user_start_[idx]; }

    // Returns the internal Minuit2 state. FitModelImpl is an incomplete type
    // here; only callers that include src/FitModelImpl.hpp can dereference it.
    FitModelImpl *impl() const { return impl_.get(); }
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
