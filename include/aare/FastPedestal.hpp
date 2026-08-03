// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/Frame.hpp"
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include <cstddef>
#include <sys/types.h>

namespace aare {

/**
 * @brief Calculate the pedestal of a series of frames. Can be used as
 * standalone but mostly used in the ClusterFinder. Internal calculations are
 * performed using double precision.
 *
 * @tparam PEDESTAL_TYPE type of the exposed mean and std
 */
template <typename PEDESTAL_TYPE> class FastPedestal {

    // Did we accumulate enough samples and updated the mean?
    bool m_ready = false; 

    uint32_t m_rows;
    uint32_t m_cols;

    uint32_t m_samples;
    double m_inv_samples;     // precompute 1/m_samples for faster division
    uint32_t m_cur_samples = 0; // number of samples accumulated so far

    // For cache locality we want to keep sum and sum2 close. Improves performance
    // for random access. 
    struct Entry {
        double sum;
        double sum2;
    };
    NDArray<Entry, 2> m_sum;

    // Cache mean since it is used over and over in the ClusterFinder
    // This optimization is related to the access pattern of the ClusterFinder
    // Relies on having more reads than pushes to the pedestal
    // But also makes sense when subtracting the pedestal from the frame
    NDArray<PEDESTAL_TYPE, 2> m_mean;

    // Helper function to convert row and column indices to a flat index
    // used to provide both row column and flat index access to the pedestal
    size_t rc_to_index(uint32_t row, uint32_t col) const {
        return (static_cast<std::size_t>(row) * m_cols) + col;
    }
  public:
    FastPedestal(uint32_t rows, uint32_t cols, uint32_t n_samples = 1000)
        : m_rows(rows), m_cols(cols), m_samples(n_samples),
          m_inv_samples(1.0 / n_samples),
          m_sum({rows, cols}, Entry{0, 0}),
          m_mean({rows, cols}, PEDESTAL_TYPE(0)) {
        assert(rows > 0 && cols > 0 && n_samples > 0);
    }
    ~FastPedestal() = default;

    const NDView<PEDESTAL_TYPE, 2> view() const { return m_mean.view(); }

    NDArray<PEDESTAL_TYPE, 2> mean() { return m_mean; }

    PEDESTAL_TYPE mean(uint32_t row, uint32_t col) const {
        return m_mean(row, col);
    }

    PEDESTAL_TYPE mean(ssize_t index) const {
        return m_mean[index];
    }

    NDArray<PEDESTAL_TYPE, 2> variance() {
        NDArray<PEDESTAL_TYPE, 2> res({m_rows, m_cols});
        for (ssize_t i = 0; i < m_sum.size(); ++i) {
            res[i] = variance(i);
        }
        return res;
    }

    PEDESTAL_TYPE variance(const uint32_t row, const uint32_t col) const {
        return variance(rc_to_index(row, col));
    }

    PEDESTAL_TYPE variance(ssize_t index) const {
        auto &entry = m_sum[index];
        auto m2 = entry.sum * m_inv_samples * entry.sum * m_inv_samples;
        return entry.sum2 * m_inv_samples - m2;
    }

    NDArray<PEDESTAL_TYPE, 2> std() {
        NDArray<PEDESTAL_TYPE, 2> res({m_rows, m_cols});
        for (ssize_t i = 0; i < m_sum.size(); ++i) {
            res[i] = std(i);
        }
        return res;
    }

    PEDESTAL_TYPE std(const uint32_t row, const uint32_t col) const {
        return std::sqrt(variance(row, col));
    }

    PEDESTAL_TYPE std(ssize_t index) const {
        return std::sqrt(variance(index));
    }


    bool ready() const { return m_ready; }

    uint32_t cur_samples() const { return m_cur_samples; }

    void clear() {
        m_sum = Entry{0., 0.};
        m_mean = PEDESTAL_TYPE(0.);
        m_ready = false;
    }


    template <typename T> void push(NDView<T, 2> frame) {
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
                const auto val = static_cast<double>(frame(row, col));
                auto &entry = m_sum(row, col);
                entry.sum += val;
                entry.sum2 += val * val;
            }
        }
        m_cur_samples += 1;

        if (m_cur_samples == m_samples) {
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

    
    /**
     * @brief Update one pixel using its flat index.
     *
     * This steady-state fast path assumes the pedestal is ready and the index
     * is valid. Assertions check those preconditions in debug builds.
     */
    template <typename T>
    void push_fast(const std::size_t index, const T value) noexcept {
        assert(m_ready);
        assert(index < static_cast<std::size_t>(m_sum.size()));

        const auto val = static_cast<double>(value);
        auto &entry = m_sum[index];
        entry.sum += val - entry.sum * m_inv_samples;
        entry.sum2 += val * val - entry.sum2 * m_inv_samples;
        m_mean[index] =
            static_cast<PEDESTAL_TYPE>(entry.sum * m_inv_samples);
    }


    template <typename T>
    void push(const uint32_t row, const uint32_t col, const T val_) {
        if (!ready()) {
            throw std::runtime_error("Pedestal is not ready, cannot push");
        }
        const auto index =
            (static_cast<std::size_t>(row) * m_cols) + col;
        push_fast(index, val_);
    }

    template <typename T>
    void push_no_update(const uint32_t row, const uint32_t col, const T val_) {
        if (!ready()) {
            throw std::runtime_error("Pedestal is not ready, cannot push");
        }
        auto val = static_cast<double>(val_);
        auto &entry = m_sum(row, col);
        entry.sum += val - entry.sum * m_inv_samples;
        entry.sum2 += val * val - entry.sum2 * m_inv_samples;
    }

    /**
     * @brief Update the mean of the pedestal. This is used after having done
     * push_no_update. It is not necessary to call this function after push.
     */
    void update_mean() {
        for (ssize_t i = 0; i < m_sum.size(); i++) {
            auto &entry = m_sum[i];
            m_mean[i] = static_cast<PEDESTAL_TYPE>(entry.sum * m_inv_samples);
        }
    }

};
} // namespace aare
