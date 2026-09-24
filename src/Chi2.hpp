// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/Models.hpp"
#include "aare/NDView.hpp"
#include <Minuit2/FCNGradientBase.h>
#include <Minuit2/FumiliFCNBase.h>
#include <array>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace aare {

namespace func {

/**
 * @brief Chi-squared objective of a 1D model and its analytic derivatives.
 *
 * Shared by the Minuit2 FCN adapters below.
 *
 *   chi2(par) = sum_i w_i * (y_i - f(x_i; par))^2
 *
 * with w_i = 1/sigma_i^2 for weighted fits (points with sigma_i == 0 are
 * skipped) and w_i = 1 otherwise.
 *
 * @tparam Model  A model struct that satisfies:
 *   - static constexpr std::size_t npar;
 *   - static double eval(double x, const std::vector<double>& par);
 *   - static void eval_and_grad(double x, const std::vector<double>& par,
 *                                double& f, std::array<double, npar>& g);
 *   - static bool is_valid(const std::vector<double>& par);
 *
 * Derivatives, with df_i/dp_k taken from Model::eval_and_grad:
 *
 *   d(chi2)/dp_k        = -2 * sum_i w_i * (y_i - f_i) * df_i/dp_k
 *   d2(chi2)/dp_j dp_k ~=  2 * sum_i w_i * df_i/dp_j * df_i/dp_k
 *
 * The second line drops the residual-weighted second derivatives of the
 * model, which is the Fumili (Gauss-Newton) approximation of the Hessian.
 *
 * Invalid model parameters do not throw; they return a large penalty (with
 * a zero gradient and unit curvature) so the minimizer can remain in control.
 *
 * @throws std::invalid_argument if par.size() != Model::npar.
 */
template <class Model> class Chi2Data1D {
  public:
    static constexpr std::size_t npar = Model::npar;
    /** @brief Number of elements of the packed symmetric Hessian. */
    static constexpr std::size_t nhess = npar * (npar + 1) / 2;
    static constexpr double invalid_penalty = 1e20;

    /** @brief An empty y_err view selects an unweighted fit. */
    Chi2Data1D(NDView<double, 1> x, NDView<double, 1> y,
               NDView<double, 1> y_err = NDView<double, 1>{})
        : x_(x), y_(y), s_(y_err), weighted_(y_err.size() > 0) {}

    double value(const std::vector<double> &par) const {
        check_size(par);
        if (!Model::is_valid(par))
            return invalid_penalty;
        return weighted_ ? value_impl<true>(par) : value_impl<false>(par);
    }

    std::vector<double> gradient(const std::vector<double> &par) const {
        check_size(par);
        std::vector<double> grad(npar, 0.0);
        if (!Model::is_valid(par))
            return grad;
        if (weighted_)
            gradient_impl<true>(par, grad);
        else
            gradient_impl<false>(par, grad);
        return grad;
    }

    /**
     * @brief Compute chi2, its gradient and the Gauss-Newton Hessian in a
     * single pass over the data.
     *
     * @param grad  Receives d(chi2)/dp_k, resized to npar.
     * @param hess  Receives the packed symmetric Hessian in the layout used by
     *              ROOT::Minuit2::FumiliFCNBase: element (j, k) with j <= k is
     *              stored at index j + k * (k + 1) / 2. Resized to nhess.
     * @return chi2 at par.
     */
    double evaluate_all(const std::vector<double> &par,
                        std::vector<double> &grad,
                        std::vector<double> &hess) const {
        check_size(par);
        grad.assign(npar, 0.0);
        hess.assign(nhess, 0.0);
        if (!Model::is_valid(par)) {
            for (std::size_t k = 0; k < npar; ++k)
                hess[k + k * (k + 1) / 2] = 1.0;
            return invalid_penalty;
        }
        return weighted_ ? evaluate_all_impl<true>(par, grad, hess)
                         : evaluate_all_impl<false>(par, grad, hess);
    }

  private:
    static void check_size(const std::vector<double> &par) {
        if (par.size() != npar) {
            throw std::invalid_argument(
                "Chi2Data1D: wrong parameter vector size.");
        }
    }

    template <bool Weighted>
    double value_impl(const std::vector<double> &par) const {
        double chi2 = 0.0;
        for (ssize_t i = 0; i < x_.size(); ++i) {
            if constexpr (Weighted) {
                const double si = s_[i];
                if (si == 0.0)
                    continue;
                const double r_i = y_[i] - Model::eval(x_[i], par);
                chi2 += (r_i * r_i) / (si * si);
            } else {
                const double r_i = y_[i] - Model::eval(x_[i], par);
                chi2 += r_i * r_i;
            }
        }
        return chi2;
    }

    template <bool Weighted>
    void gradient_impl(const std::vector<double> &par,
                       std::vector<double> &grad) const {
        std::array<double, npar> df{};
        double f_i = 0.0;
        for (ssize_t i = 0; i < x_.size(); ++i) {
            double c = 0.0;
            if constexpr (Weighted) {
                const double si = s_[i];
                if (si == 0.0)
                    continue;
                Model::eval_and_grad(x_[i], par, f_i, df);
                c = -2.0 * (y_[i] - f_i) / (si * si);
            } else {
                Model::eval_and_grad(x_[i], par, f_i, df);
                c = -2.0 * (y_[i] - f_i);
            }
            for (std::size_t k = 0; k < npar; ++k)
                grad[k] += c * df[k];
        }
    }

    template <bool Weighted>
    double evaluate_all_impl(const std::vector<double> &par,
                             std::vector<double> &grad,
                             std::vector<double> &hess) const {
        std::array<double, npar> df{};
        double f_i = 0.0;
        double chi2 = 0.0;
        for (ssize_t i = 0; i < x_.size(); ++i) {
            double w = 1.0;
            if constexpr (Weighted) {
                const double si = s_[i];
                if (si == 0.0)
                    continue;
                w = 1.0 / (si * si);
            }
            Model::eval_and_grad(x_[i], par, f_i, df);
            const double r_i = y_[i] - f_i;
            chi2 += w * r_i * r_i;
            const double c = -2.0 * w * r_i;
            for (std::size_t k = 0; k < npar; ++k) {
                grad[k] += c * df[k];
                const double h_k = 2.0 * w * df[k];
                double *column = hess.data() + k * (k + 1) / 2;
                for (std::size_t j = 0; j <= k; ++j)
                    column[j] += h_k * df[j];
            }
        }
        return chi2;
    }

    NDView<double, 1> x_;
    NDView<double, 1> y_;
    NDView<double, 1> s_;
    bool weighted_;
};

/**
 * @brief Chi-squared FCN with analytic gradient, used by Migrad.
 *
 * Providing the analytic gradient avoids the 2*npar extra function
 * evaluations per Minuit step that finite differences would need.
 */
template <class Model>
class Chi2Model1DGrad : public ROOT::Minuit2::FCNGradientBase {
  public:
    /** @brief An empty y_err view selects an unweighted fit. */
    Chi2Model1DGrad(NDView<double, 1> x, NDView<double, 1> y,
                    NDView<double, 1> y_err = NDView<double, 1>{})
        : data_(x, y, y_err) {}

    ~Chi2Model1DGrad() override = default;

    double operator()(const std::vector<double> &par) const override {
        return data_.value(par);
    }

    std::vector<double>
    Gradient(const std::vector<double> &par) const override {
        return data_.gradient(par);
    }

    /** @brief Error definition: 1.0 for chi-squared (delta_chi2 = 1 ->
     * 1-sigma). */
    double Up() const override { return 1.0; }

  private:
    Chi2Data1D<Model> data_;
};

/**
 * @brief Chi-squared FCN for Fumili.
 *
 * EvaluateAll fills the value, gradient and Gauss-Newton Hessian cached by
 * FumiliFCNBase in one pass over the data, so Fumili never needs numerical
 * derivatives.
 */
template <class Model>
class Chi2Model1DFumili : public ROOT::Minuit2::FumiliFCNBase {
  public:
    /** @brief An empty y_err view selects an unweighted fit. */
    Chi2Model1DFumili(NDView<double, 1> x, NDView<double, 1> y,
                      NDView<double, 1> y_err = NDView<double, 1>{})
        : ROOT::Minuit2::FumiliFCNBase(static_cast<unsigned int>(Model::npar)),
          data_(x, y, y_err) {}

    double operator()(const std::vector<double> &par) const override {
        return data_.value(par);
    }

    /** @brief Error definition: 1.0 for chi-squared (delta_chi2 = 1 ->
     * 1-sigma). */
    double Up() const override { return 1.0; }

    void EvaluateAll(const std::vector<double> &par) override {
        SetFCNValue(data_.evaluate_all(par, Gradient(), Hessian()));
    }

  private:
    Chi2Data1D<Model> data_;
};

} // namespace func

} // namespace aare
