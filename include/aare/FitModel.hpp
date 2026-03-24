// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <type_traits>
#include "aare/Models.hpp"

#include "Minuit2/MnUserParameters.h"
#include "Minuit2/MnStrategy.h"

namespace aare {


template <typename Model>
class FitModel {
    ROOT::Minuit2::MnUserParameters upar_;
    ROOT::Minuit2::MnStrategy strategy_;
    unsigned int max_calls_;
    double tolerance_;
    bool compute_errors_;

    std::array<bool, Model::npar> user_fixed_{};
    std::array<bool, Model::npar> user_start_{};

public:
    static constexpr std::size_t npar = Model::npar;

    /**
     * @brief Construct a fit model with sensible defaults.
     *
     * @param strategy        Minuit2 strategy level (0 = fast/gradient, 1 = default).
     * @param max_calls       Maximum FCN calls per pixel minimisation.
     * @param tolerance       Minuit2 EDM tolerance.
     * @param compute_errors  If true, run MnHesse after minimisation.
     */
    FitModel(unsigned int strategy = 0,
             unsigned int max_calls = 100,
             double tolerance = 0.5,
             bool compute_errors = false)
        : strategy_(strategy),
          max_calls_(max_calls),
          tolerance_(tolerance),
          compute_errors_(compute_errors)
    {
        for(std::size_t i = 0; i < npar; ++i){
            const auto pi = Model::param_info[i];
            const bool has_lo = std::isfinite(pi.default_lo);
            const bool has_hi = std::isfinite(pi.default_hi);

            // Add parameters and valid bounds
            if (has_lo && has_hi){
                upar_.Add(pi.name, 0.0, 1.0, pi.default_lo, pi.default_hi);
            } else if (has_lo) {
                upar_.Add(pi.name, 0.0, 1.0, pi.default_lo, 1e6);
            } else {
                upar_.Add(pi.name, 0.0, 1.0);
            }
        } 
    }

    /** @brief Set lower and upper bounds for parameter idx.*/
    void SetParLimits(unsigned int idx, double lo, double hi) { 
        upar_.SetLimits(idx, lo, hi); 
    }

    /**
     * @brief Fix parameter idx at value val.
     *
     * Excluded from minimisation.  Automatic estimates will not touch it.
     */
    void FixParameter(unsigned int idx, double val) { 
        upar_.SetValue(idx, val); 
        upar_.Fix(idx);
        user_start_[idx] = true;
        user_fixed_[idx] = true;
    }
    
    /** @brief Release a previously fixed parameter, re-enabling auto estimates. */
    void ReleaseParameter(unsigned int idx) {
        upar_.Release(idx);
        user_fixed_[idx] = false;
    }

    /** @brief Set an explicit starting value for parameter idx.*/
    void SetParameter(unsigned int idx, double val) {
        upar_.SetValue(idx, val);
        user_start_[idx] = true;
    }
    
    void SetMaxCalls(unsigned int n)  { max_calls_ = n; }
    void SetTolerance(double t)       { tolerance_ = t; }  
    void SetComputeErrors(bool b)     { compute_errors_ = b; }

    // accessors
    const ROOT::Minuit2::MnUserParameters& upar()    const { return upar_; }
    const ROOT::Minuit2::MnStrategy&       strategy() const { return strategy_; }
    unsigned int max_calls()      const { return max_calls_; }
    double       tolerance()      const { return tolerance_; }
    bool         compute_errors() const { return compute_errors_; }
    bool         is_user_fixed(unsigned int idx) const { return user_fixed_[idx]; }
    bool         is_user_start(unsigned int idx) const { return user_start_[idx]; }
};


} // namespace aare