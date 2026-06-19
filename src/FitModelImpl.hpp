// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "Minuit2/MnStrategy.h"
#include "Minuit2/MnUserParameters.h"

namespace aare {

// Private implementation struct for FitModel<Model>.
// Never installed; only included by src/Fit.cpp.
struct FitModelImpl {
    ROOT::Minuit2::MnUserParameters upar;
    ROOT::Minuit2::MnStrategy strategy;

    explicit FitModelImpl(unsigned int strategy_level)
        : strategy(strategy_level) {}

    FitModelImpl(const FitModelImpl &) = default;
    FitModelImpl &operator=(const FitModelImpl &) = default;
};

} // namespace aare
