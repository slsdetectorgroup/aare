// SPDX-License-Identifier: MPL-2.0
#pragma once

// Small fixed-size linear algebra of the built-in minimizers. Everything
// lives on the stack and the sizes are compile-time constants, so the loops
// unroll. Function templates cannot deduce N from the std::array aliases
// (int against std::size_t), hence the explicit compress<N>(...).

#include "aare/defs.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace aare::detail {

/** @brief Fixed-size vector. */
template <int N> using Vec = std::array<double, N>;

/** @brief Fixed-size R x C matrix, square by default. */
template <int R, int C = R> using Mat = std::array<std::array<double, C>, R>;

/**
 * @brief Symmetric N x N matrix. Only the upper triangle is stored and
 * updated; (i, j) and (j, i) name the same element.
 */
template <int N> struct SymMat {
    Mat<N> upper{}; // [i][j] with i <= j; the rest is never read

    double &operator()(int i, int j) {
        return upper[std::min(i, j)][std::max(i, j)];
    }
    double operator()(int i, int j) const {
        return upper[std::min(i, j)][std::max(i, j)];
    }

    /**
     * @brief Adds w * x x^T, for x indexable with N elements. It is inlined
     * because it runs once per data point.
     */
    template <typename X>
    ALWAYS_INLINE void rank1_update(const X &x, double w = 1.0) {
        for (int a = 0; a < N; ++a) {
            const double xa = x[a] * w;
            for (int b = a; b < N; ++b)
                upper[a][b] += xa * x[b];
        }
    }
};

/**
 * @brief Cholesky factorisation A = L L^T of the leading m x m block of a
 * symmetric positive-definite matrix, for any number of solves with it.
 *
 * The methods clamp m to N. The callers never exceed it, but with the bound
 * visible gcc's range analysis no longer reports subscripts of the
 * one-parameter instantiations as out of range.
 */
template <int N> class Cholesky {
  public:
    /** @brief Factorises; false when the block is not positive definite. */
    bool factor(int m, const SymMat<N> &A) {
        m = std::min(m, N);
        m_ = 0;
        for (int i = 0; i < m; ++i) {
            diag_[i] = A.upper[i][i];
            for (int j = 0; j <= i; ++j) {
                double sum = A.upper[j][i];
                for (int k = 0; k < j; ++k)
                    sum -= L_[i][k] * L_[j][k];
                if (i == j) {
                    if (!(sum > 0.0))
                        return false;
                    L_[i][i] = std::sqrt(sum);
                } else {
                    L_[i][j] = sum / L_[j][j];
                }
            }
        }
        m_ = m;
        return true;
    }

    /** @brief Solves A z = b for the first m entries of b and z. */
    void solve(const Vec<N> &b, Vec<N> &z) const {
        const int m = std::min(m_, N);
        Vec<N> t{};
        for (int i = 0; i < m; ++i) {
            double sum = b[i];
            for (int k = 0; k < i; ++k)
                sum -= L_[i][k] * t[k];
            t[i] = sum / L_[i][i];
        }
        for (int i = m - 1; i >= 0; --i) {
            double sum = t[i];
            for (int k = i + 1; k < m; ++k)
                sum -= L_[k][i] * z[k];
            z[i] = sum / L_[i][i];
        }
    }

    /**
     * @brief True when every pivot satisfies L_ii^2 >= ratio * A_ii. A pivot
     * far below its diagonal element costs a solution about
     * log10(A_ii / L_ii^2) digits.
     */
    bool pivots_at_least(double ratio) const {
        const int m = std::min(m_, N);
        for (int i = 0; i < m; ++i) {
            if (!(L_[i][i] * L_[i][i] >= ratio * diag_[i]))
                return false;
        }
        return true;
    }

    /** @brief The first m diagonal elements of A^-1. */
    void inverse_diag(Vec<N> &d) const {
        const int m = std::min(m_, N);
        Vec<N> e{};
        Vec<N> col{};
        for (int j = 0; j < m; ++j) {
            for (int i = 0; i < m; ++i)
                e[i] = (i == j) ? 1.0 : 0.0;
            solve(e, col);
            d[j] = col[j];
        }
    }

  private:
    int m_ = 0;
    Mat<N> L_{};    // lower triangle
    Vec<N> diag_{}; // diagonal of A
};

/**
 * @brief Packs the rows and columns of H that drop does not mark, among the
 * first n, into the leading block of A. Returns the size of that block.
 */
template <int N>
int compress(int n, const SymMat<N> &H, const std::array<bool, N> &drop,
             SymMat<N> &A) {
    n = std::min(n, N);
    int m = 0;
    for (int a = 0; a < n; ++a) {
        if (drop[a])
            continue;
        int mc = m;
        for (int c = a; c < n; ++c) {
            if (drop[c])
                continue;
            A.upper[m][mc++] = H.upper[a][c];
        }
        ++m;
    }
    return m;
}

/**
 * @brief Packs the entries of g that drop does not mark, among the first n,
 * into the leading entries of b. Returns their number.
 */
template <int N>
int compress(int n, const Vec<N> &g, const std::array<bool, N> &drop,
             Vec<N> &b) {
    n = std::min(n, N);
    int m = 0;
    for (int a = 0; a < n; ++a) {
        if (!drop[a])
            b[m++] = g[a];
    }
    return m;
}

} // namespace aare::detail
