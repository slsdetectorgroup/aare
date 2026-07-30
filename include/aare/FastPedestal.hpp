// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/Frame.hpp"
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include <cstddef>

namespace aare {

/**
 * @brief Calculate the pedestal of a series of frames. Can be used as
 * standalone but mostly used in the ClusterFinder.
 *
 * @tparam SUM_TYPE type of the sum
 */
template <typename SUM_TYPE = double> class FastPedestal {
    // TODO! Force floating point sum type?
    // how does the internal calculation work with integers?

    bool m_ready = false; 

    uint32_t m_rows;
    uint32_t m_cols;

    uint32_t m_samples;
    SUM_TYPE m_inv_samples;     // precompute 1/m_samples for faster division
    uint32_t m_cur_samples = 0; // TODO! do we need this when we have m_samples?

    // for cache we want to keep sum and sum2 close
    struct Entry {
        SUM_TYPE sum;
        SUM_TYPE sum2;
    };
    // TODO! in case of int needs to be changed to uint64_t
    NDArray<Entry, 2> m_sum;

    // Cache mean since it is used over and over in the ClusterFinder
    // This optimization is related to the access pattern of the ClusterFinder
    // Relies on having more reads than pushes to the pedestal
    NDArray<SUM_TYPE, 2> m_mean;

    // Cache std. Only refreshed via update_std() to keep push() cheap.
    NDArray<SUM_TYPE, 2> m_std;

  public:
    FastPedestal(uint32_t rows, uint32_t cols, uint32_t n_samples = 1000)
        : m_rows(rows), m_cols(cols), m_samples(n_samples),
          m_inv_samples(1.0 / n_samples),
          m_sum(NDArray<Entry, 2>({rows, cols})),
          m_mean(NDArray<SUM_TYPE, 2>({rows, cols})),
          m_std(NDArray<SUM_TYPE, 2>({rows, cols})) {
        assert(rows > 0 && cols > 0 && n_samples > 0);
        m_sum = Entry{SUM_TYPE(0), SUM_TYPE(0)};
        m_mean = SUM_TYPE(0);
        m_std = SUM_TYPE(0);
    }
    ~FastPedestal() = default;

    NDArray<SUM_TYPE, 2> mean() { return m_mean; }

    const NDView<SUM_TYPE, 2> view() const { return m_mean.view(); }

    SUM_TYPE mean(const uint32_t row, const uint32_t col) const {
        return m_mean(row, col);
    }

    NDArray<SUM_TYPE, 2> cached_std() { return m_std; }

    SUM_TYPE cached_std(const uint32_t row, const uint32_t col) const {
        return m_std(row, col);
    }

    SUM_TYPE variance(const uint32_t row, const uint32_t col) const {
        auto &entry = m_sum(row, col);
        auto m2 = entry.sum * m_inv_samples * entry.sum * m_inv_samples;
        return entry.sum2 * m_inv_samples - m2;
    }

    NDArray<SUM_TYPE, 2> variance() {
        NDArray<SUM_TYPE, 2> res({m_rows, m_cols});
        for (ssize_t row = 0; row < m_rows; ++row) {
            for (ssize_t col = 0; col < m_cols; ++col) {
                res(row, col) = variance(row, col);
            }
        }
        return res;
    }

    SUM_TYPE std(const uint32_t row, const uint32_t col) const {
        return std::sqrt(variance(row, col));
    }

    NDArray<SUM_TYPE, 2> std() {
        NDArray<SUM_TYPE, 2> res({m_rows, m_cols});
        for (ssize_t row = 0; row < m_rows; ++row) {
            for (ssize_t col = 0; col < m_cols; ++col) {
                res(row, col) = std(row, col);
            }
        }
        return res;
    }

    bool ready() { return m_ready; }

    uint32_t cur_samples() { return m_cur_samples; }

    void clear() {
        m_sum = Entry{SUM_TYPE(0), SUM_TYPE(0)};
        m_mean = SUM_TYPE(0);
        m_std = SUM_TYPE(0);
        m_ready = false;
    }


    template <typename T> void push(NDView<T, 2> frame) {
        assert(frame.size() == m_rows * m_cols);

        // TODO! move away from m_rows, m_cols
        if (frame.shape() != std::array<ssize_t, 2>{m_rows, m_cols}) {
            throw std::runtime_error(
                "Frame shape does not match pedestal shape");
        }

        for (size_t row = 0; row < m_rows; row++) {
            for (size_t col = 0; col < m_cols; col++) {
                push<T>(row, col, frame(row, col));
            }
        }
    }
    template <typename T> void push_init(NDView<T, 2> frame) {
        assert(frame.size() == m_rows * m_cols);

        // TODO! move away from m_rows, m_cols
        if (frame.shape() != std::array<ssize_t, 2>{m_rows, m_cols}) {
            throw std::runtime_error(
                "Frame shape does not match pedestal shape");
        }

        // if already full, throw an error
        if (m_cur_samples == m_samples) {
            throw std::runtime_error("Pedestal is full");
        }

        for (size_t row = 0; row < m_rows; row++) {
            for (size_t col = 0; col < m_cols; col++) {
                const auto val = static_cast<SUM_TYPE>(frame(row, col));
                auto &entry = m_sum(row, col);
                entry.sum += val;
                entry.sum2 += val * val;
            }
        }
        m_cur_samples += 1;

        if (m_cur_samples == m_samples) {
            update_std();
            update_mean();
            m_ready = true;
        }
    }

    template <typename T> void push(Frame &frame) {
        assert(frame.rows() == static_cast<size_t>(m_rows) &&
               frame.cols() == static_cast<size_t>(m_cols));
        push<T>(frame.view<T>());
    }

    // getter functions
    uint32_t rows() const { return m_rows; }
    uint32_t cols() const { return m_cols; }
    uint32_t n_samples() const { return m_samples; }

    // pixel level operations (should be refactored to allow users to implement
    // their own pixel level operations)
    template <typename T>
    void push(const uint32_t row, const uint32_t col, const T val_) {
        if (!ready()) {
            throw std::runtime_error("Pedestal is not ready, cannot push");
        }
        SUM_TYPE val = static_cast<SUM_TYPE>(val_);
        auto &entry = m_sum(row, col);
        entry.sum += val - entry.sum * m_inv_samples;
        entry.sum2 += val * val - entry.sum2 * m_inv_samples;
        m_mean(row, col) = entry.sum * m_inv_samples;
    }

    template <typename T>
    void push_no_update(const uint32_t row, const uint32_t col, const T val_) {
        if (!ready()) {
            throw std::runtime_error("Pedestal is not ready, cannot push");
        }
        SUM_TYPE val = static_cast<SUM_TYPE>(val_);
        auto &entry = m_sum(row, col);
        entry.sum += val - entry.sum * m_inv_samples;
        entry.sum2 += val * val - entry.sum2 * m_inv_samples;
    }

    /**
     * @brief Update the mean of the pedestal. This is used after having done
     * push_no_update. It is not necessary to call this function after push.
     */
    void update_mean() {
        for (size_t row = 0; row < m_rows; row++) {
            for (size_t col = 0; col < m_cols; col++) {
                const auto &entry = m_sum(row, col);
                m_mean(row, col) = entry.sum * m_inv_samples;
            }
        }
    }

    /**
     * @brief Refresh the cached std for all pixels from the current sums.
     * Kept separate from push() so that pushes stay cheap; call this before
     * reading cached_std().
     */
    void update_std() {
        for (size_t row = 0; row < m_rows; row++) {
            for (size_t col = 0; col < m_cols; col++) {
                m_std(row, col) = std(row, col);
            }
        }
    }
};
} // namespace aare
