// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include "aare/defs.hpp"

#include <algorithm>
#include <cstdint>
#include <fmt/format.h>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace aare {

/**
 * @brief Find clusters of variable size by flood filling connected pixels
 * above a threshold.
 *
 * The image is scanned in row-major order. A pixel above the seed threshold
 * that does not already belong to a cluster starts a new cluster, which then
 * grows into every 4- or 8-connected neighbour above the neighbour threshold.
 * Without a noise map the thresholds are fixed values: the threshold given
 * to the constructor for seeds and, once set_neighbour_threshold() is used,
 * a separate one for neighbours. With a noise map they are per-pixel
 * multiples of the noise: the seed factor times the noise for seeds and the
 * neighbour factor times the noise for neighbours, both 5 by default.
 *
 * The clusters are collected in a Hits structure: one Hit per cluster with
 * the pixel count, the summed energy and the position and value of the
 * maximum pixel, plus flat arrays with the coordinates and values of every
 * cluster pixel. The pixels of a hit are a contiguous slice of those arrays
 * in fill order, starting with the seed. Hits are appended in the order
 * their seeds are encountered and accumulate across frames until
 * steal_hits() or clear_hits() is called.
 *
 * Pixels can be excluded with set_mask(): a masked pixel never seeds a
 * cluster and is never added to one, so clusters grow around it as they
 * do around the image border. Masked pixels are never modified and show
 * up as -1 in labeled().
 *
 * The input image is left untouched unless set_erase_clusters(true) is set,
 * in which case every cluster pixel, and also every sub-threshold neighbour
 * the fill examined around it, is set to zero as it is visited. The ring is
 * included so that a pedestal updated from the erased frame sees no charge
 * that leaked out of the cluster. A zeroed pixel compares as zero for the
 * rest of the frame, which only matters for negative thresholds.
 *
 * @tparam T Pixel type of the image and the noise map.
 */
template <typename T> class VarClusterFinder {
  public:
    /**
     * @brief One cluster found by find_clusters(). Its pixels are the
     * entries [offset, offset + size) of the pixel arrays in Hits.
     */
    struct Hit {
        int32_t size{};   ///< Number of pixels in the cluster
        int16_t row{};    ///< Row of the maximum pixel
        int16_t col{};    ///< Column of the maximum pixel
        T energy{};       ///< Sum over all pixels in the cluster
        T max{};          ///< Value of the maximum pixel
        int64_t offset{}; ///< Index of the first pixel in Hits::rows etc.
    };

    /**
     * @brief Clusters found by find_clusters(): one Hit per cluster and
     * one entry per cluster pixel in rows, cols and values.
     */
    struct Hits {
        std::vector<Hit> hits;     ///< One record per cluster
        std::vector<int16_t> rows; ///< Row of every cluster pixel
        std::vector<int16_t> cols; ///< Column of every cluster pixel
        std::vector<T> values;     ///< Value of every cluster pixel

        /** @brief Number of clusters. */
        size_t size() const { return hits.size(); }
        bool empty() const { return hits.empty(); }
        /** @brief Number of cluster pixels over all clusters. */
        size_t n_pixels() const { return rows.size(); }
        void clear() {
            hits.clear();
            rows.clear();
            cols.clear();
            values.clear();
        }
        void reserve(size_t n_hits, size_t n_pixels) {
            hits.reserve(n_hits);
            rows.reserve(n_pixels);
            cols.reserve(n_pixels);
            values.reserve(n_pixels);
        }
    };

  private:
    struct FixedThreshold {
        T seed_value;
        T neighbour_value;
        T seed(ssize_t) const { return seed_value; }
        T neighbour(ssize_t) const { return neighbour_value; }
    };

    struct NoiseThreshold {
        const T *noise;
        T seed_factor;
        T neighbour_factor;
        T seed(ssize_t i) const { return seed_factor * noise[i]; }
        T neighbour(ssize_t i) const { return neighbour_factor * noise[i]; }
    };

    static constexpr size_t initial_hit_capacity = 2000;
    static constexpr size_t initial_pixel_capacity = 8 * initial_hit_capacity;

    Shape<2> shape_;
    T threshold_;
    std::optional<T> neighbour_threshold_; // follows threshold_ when unset
    std::optional<NDArray<T, 2>> noise_map_;
    // Label image template: -1 on masked pixels, 0 elsewhere
    std::optional<NDArray<int32_t, 2>> masked_labels_;
    T seed_factor_ = 5;
    T neighbour_factor_ = 5;
    int connectivity_ = 8;
    bool erase_clusters_ = false;
    NDArray<int32_t, 2> labels_;
    Hits hits_;

  public:
    /**
     * @brief Construct a finder for images of the given shape.
     * @param shape Image shape as (rows, columns); each at most 32767.
     * @param threshold Fixed seed threshold, also used for neighbours until
     * set_neighbour_threshold() is called; ignored while a noise map is set.
     * @throws std::runtime_error if the shape is empty or does not fit the
     * 16 bit coordinates of Hit.
     */
    VarClusterFinder(Shape<2> shape, T threshold)
        : shape_(shape), threshold_(threshold), labels_(shape, 0) {
        constexpr ssize_t max_coord = std::numeric_limits<int16_t>::max();
        if (shape[0] < 1 || shape[1] < 1 || shape[0] > max_coord ||
            shape[1] > max_coord)
            throw std::runtime_error(
                LOCATION +
                fmt::format("image shape ({}, {}) must be between (1, 1) and "
                            "({}, {})",
                            shape[0], shape[1], max_coord, max_coord));
        hits_.reserve(initial_hit_capacity, initial_pixel_capacity);
    }

    /**
     * @brief Return a copy of the label image of the last processed frame.
     *
     * Pixels outside clusters are 0; cluster pixels hold the 1-based index
     * of their cluster within that frame, in the order the hits were added;
     * masked pixels are -1.
     */
    NDArray<int32_t, 2> labeled() const { return labels_; }

    /**
     * @brief Use per-pixel thresholds derived from a noise map instead of
     * the fixed threshold. The map is copied.
     * @throws std::runtime_error if the shape does not match.
     */
    void set_noise_map(NDView<const T, 2> noise_map) {
        check_shape(noise_map.shape(), "noise map");
        noise_map_.emplace(noise_map);
    }

    /** @brief Drop the noise map and use the fixed threshold again. */
    void clear_noise_map() { noise_map_.reset(); }

    /** @brief The noise map, or nothing while the fixed threshold is used. */
    const std::optional<NDArray<T, 2>> &noise_map() const { return noise_map_; }

    /**
     * @brief Exclude the pixels where mask is true: they never seed a
     * cluster, are never added to one and are never modified. The mask is
     * copied.
     * @throws std::runtime_error if the shape does not match.
     */
    void set_mask(NDView<const bool, 2> mask) {
        check_shape(mask.shape(), "mask");
        NDArray<int32_t, 2> labels(shape_, 0);
        const bool *masked = mask.data();
        int32_t *out = labels.data();
        for (ssize_t i = 0; i < shape_[0] * shape_[1]; ++i)
            out[i] = masked[i] ? -1 : 0;
        masked_labels_.emplace(std::move(labels));
    }

    /** @brief Remove the mask. */
    void clear_mask() { masked_labels_.reset(); }

    /** @brief The mask as a boolean image, or nothing when no mask is set. */
    std::optional<NDArray<bool, 2>> mask() const {
        if (!masked_labels_)
            return std::nullopt;
        NDArray<bool, 2> out(shape_, false);
        const int32_t *in = masked_labels_->data();
        bool *masked = out.data();
        for (ssize_t i = 0; i < shape_[0] * shape_[1]; ++i)
            masked[i] = in[i] < 0;
        return out;
    }

    /** @brief Fixed seed threshold, used without a noise map. */
    void set_threshold(T threshold) { threshold_ = threshold; }
    /** @brief The fixed seed threshold. */
    T threshold() const { return threshold_; }

    /**
     * @brief Fixed neighbour threshold, used without a noise map. Until it
     * is set, and again after clear_neighbour_threshold(), neighbours use
     * the seed threshold.
     */
    void set_neighbour_threshold(T threshold) {
        neighbour_threshold_ = threshold;
    }
    /** @brief Let neighbours use the seed threshold again. */
    void clear_neighbour_threshold() { neighbour_threshold_.reset(); }
    /** @brief The neighbour threshold in effect without a noise map. */
    T neighbour_threshold() const {
        return neighbour_threshold_.value_or(threshold_);
    }

    /** @brief Seed threshold in units of the noise map (default 5). */
    void set_seed_factor(T factor) { seed_factor_ = factor; }
    /** @brief The seed threshold factor. */
    T seed_factor() const { return seed_factor_; }

    /** @brief Neighbour threshold in units of the noise map (default 5). */
    void set_neighbour_factor(T factor) { neighbour_factor_ = factor; }
    /** @brief The neighbour threshold factor. */
    T neighbour_factor() const { return neighbour_factor_; }

    /**
     * @brief Grow clusters through 4 or 8 connected neighbours (default 8).
     * @throws std::runtime_error for any other value.
     */
    void set_connectivity(int connectivity) {
        if (connectivity != 4 && connectivity != 8)
            throw std::runtime_error(LOCATION +
                                     "connectivity should be either 4 or 8");
        connectivity_ = connectivity;
    }
    /** @brief The connectivity, 4 or 8. */
    int connectivity() const { return connectivity_; }

    /**
     * @brief Erase the clusters from the input image while processing a
     * frame (default off): every cluster pixel and the sub-threshold
     * neighbours around it are set to zero, so that a pedestal updated from
     * the frame afterwards is not biased by the clusters.
     */
    void set_erase_clusters(bool erase) { erase_clusters_ = erase; }
    /** @brief Whether clusters are erased from the input image. */
    bool erase_clusters() const { return erase_clusters_; }

    /**
     * @brief Find the clusters in one frame and append them to the hits.
     * @param img Image of the shape given to the constructor.
     * @throws std::runtime_error if the shape does not match.
     */
    void find_clusters(NDView<T, 2> img) {
        check_shape(img.shape(), "image");
        if (noise_map_)
            find_clusters_impl(img,
                               NoiseThreshold{noise_map_->data(), seed_factor_,
                                              neighbour_factor_});
        else
            find_clusters_impl(
                img, FixedThreshold{threshold_, neighbour_threshold()});
    }

    /** @brief Move out all accumulated hits and reset the finder. */
    Hits steal_hits() {
        Hits tmp;
        std::swap(tmp, hits_);
        hits_.reserve(initial_hit_capacity, initial_pixel_capacity);
        return tmp;
    }

    /** @brief Discard all accumulated hits. */
    void clear_hits() { hits_.clear(); }

    /** @brief Number of hits currently held by the finder. */
    size_t total_clusters() const { return hits_.size(); }

  private:
    void check_shape(const Shape<2> &shape, const char *what) const {
        if (shape != shape_)
            throw std::runtime_error(
                LOCATION +
                fmt::format("{} shape ({}, {}) does not match the finder "
                            "shape ({}, {})",
                            what, shape[0], shape[1], shape_[0], shape_[1]));
    }

    template <class Threshold>
    void find_clusters_impl(NDView<T, 2> img, const Threshold &thr) {
        const ssize_t n_pixels = shape_[0] * shape_[1];
        const ssize_t n_cols = shape_[1];
        T *const pixels = img.data();
        int32_t *const labels = labels_.data();
        if (masked_labels_)
            std::copy_n(masked_labels_->data(), n_pixels, labels);
        else
            std::fill_n(labels, n_pixels, 0);

        int32_t label = 0;
        auto seed_range = [&](ssize_t begin, ssize_t end) {
            for (ssize_t idx = begin; idx < end; ++idx) {
                if (!(pixels[idx] > thr.seed(idx)) || labels[idx] != 0)
                    continue;
                ++label;
                fill(pixels, labels, thr, label,
                     static_cast<int32_t>(idx / n_cols),
                     static_cast<int32_t>(idx % n_cols));
            }
        };

        // Most pixels are below threshold. A vectorisable check per block
        // skips them without entering the per-pixel loop.
        constexpr ssize_t block = 32;
        ssize_t start = 0;
        for (; start + block <= n_pixels; start += block) {
            bool any = false;
            for (ssize_t k = 0; k < block; ++k)
                any |= pixels[start + k] > thr.seed(start + k);
            if (any)
                seed_range(start, start + block);
        }
        seed_range(start, n_pixels);
    }

    // Out of line so that the register pressure of the fill does not spill
    // the counters of the scan loop above.
    template <class Threshold>
    NEVER_INLINE void fill(T *pixels, int32_t *labels, const Threshold &thr,
                           int32_t label, int32_t seed_row, int32_t seed_col) {
        const int32_t n_rows = static_cast<int32_t>(shape_[0]);
        const int32_t n_cols = static_cast<int32_t>(shape_[1]);
        const bool diagonals = connectivity_ == 8;

        Hit &hit = hits_.hits.emplace_back();
        hit.offset = static_cast<int64_t>(hits_.rows.size());
        accept(pixels, labels, label, hit,
               static_cast<ssize_t>(seed_row) * n_cols + seed_col, seed_row,
               seed_col);

        // The pixel arrays double as the work queue: every accepted pixel
        // is appended to them, and its neighbours are visited once the
        // queue reaches it.
        for (size_t q = static_cast<size_t>(hit.offset); q < hits_.rows.size();
             ++q) {
            const int32_t row = hits_.rows[q];
            const int32_t col = hits_.cols[q];
            const ssize_t idx = static_cast<ssize_t>(row) * n_cols + col;
            const bool up = row > 0;
            const bool down = row < n_rows - 1;
            const bool left = col > 0;
            const bool right = col < n_cols - 1;
            if (left)
                visit(pixels, labels, thr, label, hit, idx - 1, row, col - 1);
            if (right)
                visit(pixels, labels, thr, label, hit, idx + 1, row, col + 1);
            if (up)
                visit(pixels, labels, thr, label, hit, idx - n_cols, row - 1,
                      col);
            if (down)
                visit(pixels, labels, thr, label, hit, idx + n_cols, row + 1,
                      col);
            if (diagonals) {
                if (up && right)
                    visit(pixels, labels, thr, label, hit, idx - n_cols + 1,
                          row - 1, col + 1);
                if (down && left)
                    visit(pixels, labels, thr, label, hit, idx + n_cols - 1,
                          row + 1, col - 1);
                if (up && left)
                    visit(pixels, labels, thr, label, hit, idx - n_cols - 1,
                          row - 1, col - 1);
                if (down && right)
                    visit(pixels, labels, thr, label, hit, idx + n_cols + 1,
                          row + 1, col + 1);
            }
        }
    }

    // Forced inline: gcc otherwise calls these out of line from the eight
    // neighbour sites in fill.
    template <class Threshold>
    ALWAYS_INLINE void visit(T *pixels, int32_t *labels, const Threshold &thr,
                             int32_t label, Hit &hit, ssize_t idx, int32_t r,
                             int32_t c) {
        if (labels[idx] != 0)
            return;
        if (pixels[idx] > thr.neighbour(idx))
            accept(pixels, labels, label, hit, idx, r, c);
        else if (erase_clusters_)
            pixels[idx] = T{};
    }

    // Add a pixel to the cluster: label it, append it to the pixel arrays
    // and update the hit record.
    ALWAYS_INLINE void accept(T *pixels, int32_t *labels, int32_t label,
                              Hit &hit, ssize_t idx, int32_t row, int32_t col) {
        const T value = pixels[idx];
        labels[idx] = label;
        hits_.rows.push_back(static_cast<int16_t>(row));
        hits_.cols.push_back(static_cast<int16_t>(col));
        hits_.values.push_back(value);
        if (hit.size == 0 || value > hit.max) {
            hit.max = value;
            hit.row = static_cast<int16_t>(row);
            hit.col = static_cast<int16_t>(col);
        }
        ++hit.size;
        hit.energy += value;
        if (erase_clusters_)
            pixels[idx] = T{};
    }
};

} // namespace aare
