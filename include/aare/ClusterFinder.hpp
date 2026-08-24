// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/ClusterFile.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/Dtype.hpp"
#include "aare/FastPedestal.hpp"
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include "aare/Pedestal.hpp"
#include "aare/defs.hpp"
#include <cstddef>

namespace aare {

template <typename ClusterType,
          typename = std::enable_if_t<is_cluster_v<ClusterType>>>
struct no_2x2_cluster {
    constexpr static bool value =
        ClusterType::cluster_size_x > 2 && ClusterType::cluster_size_y > 2;
};

/**
 * @brief Find fixed-size photon clusters using a per-pixel pedestal and noise
 * threshold.
 * @tparam ClusterType Output cluster type; both dimensions must exceed 2.
 * @tparam FRAME_TYPE Input pixel type.
 * @tparam PEDESTAL_TYPE Type used for pedestal and threshold calculations.
 */
template <typename ClusterType = Cluster<int32_t, 3, 3>,
          typename FRAME_TYPE = uint16_t, typename PEDESTAL_TYPE = double,
          typename = std::enable_if_t<no_2x2_cluster<ClusterType>::value>>
class ClusterFinder {
    Shape<2> m_image_size;
    PEDESTAL_TYPE m_nSigma;
    const PEDESTAL_TYPE c2;
    const PEDESTAL_TYPE c3;
    FastPedestal<PEDESTAL_TYPE> m_pedestal;
    ClusterVector<ClusterType> m_clusters;

    static const uint8_t ClusterSizeX = ClusterType::cluster_size_x;
    static const uint8_t ClusterSizeY = ClusterType::cluster_size_y;
    using CT = typename ClusterType::value_type;

    NDArray<PEDESTAL_TYPE, 2> m_threshold;
    NDArray<PEDESTAL_TYPE, 2> m_pd_corrected_frame;

  public:
    /**
     * @brief Construct a cluster finder with an empty pedestal.
     * @param image_size Image shape as (rows, columns).
     * @param nSigma Per-pixel noise threshold multiplier.
     * @param capacity Initial cluster-vector capacity.
     */
    ClusterFinder(Shape<2> image_size, PEDESTAL_TYPE nSigma = 5.0,
                  size_t capacity = 1000000)
        : m_image_size(image_size), m_nSigma(nSigma),
          c2(sqrt((ClusterSizeY + 1) / 2 * (ClusterSizeX + 1) / 2)),
          c3(sqrt(ClusterSizeX * ClusterSizeY)),
          m_pedestal(image_size[0], image_size[1]), m_clusters(capacity),
          m_threshold({image_size[0], image_size[1]}, 0),
          m_pd_corrected_frame({image_size[0], image_size[1]}, 0) {
        LOG(logDEBUG) << "ClusterFinder: "
                      << "image_size: " << image_size[0] << "x" << image_size[1]
                      << ", nSigma: " << nSigma << ", capacity: " << capacity;
    }

    /**
     * @brief Set the noise multiplier used for threshold calculation and
     * recompute the threshold.
     * @param nSigma New per-pixel noise multiplier.
     */
    void set_nSigma(PEDESTAL_TYPE nSigma) {
        m_nSigma = nSigma;
        update_threshold();
    }

    /** @brief Return the current noise multiplier used for threshold
     * calculation. */
    PEDESTAL_TYPE get_nSigma() const { return m_nSigma; }

    /**
     * @brief Add a dark frame to the pedestal estimator.
     *
     * The threshold is initialized automatically when the pedestal first
     * becomes ready. Later frames update the ready pedestal.
     * @param frame Dark frame matching the configured image shape.
     * @throws std::runtime_error if the frame shape does not match.
     */
    void push_pedestal_frame(NDView<FRAME_TYPE, 2> frame) {
        if (!m_pedestal.ready()) {
            m_pedestal.push_init(frame);
            // Initialize the threshold when the pedestal becomes ready.
            if (m_pedestal.ready()) {
                update_threshold();
            }
        } else {
            m_pedestal.push(frame);
        }
    }

    /** @brief Return a copy of the per-pixel pedestal mean. */
    NDArray<PEDESTAL_TYPE, 2> pedestal() { return m_pedestal.mean(); }

    /** @brief Return the per-pixel pedestal standard deviation (noise). */
    NDArray<PEDESTAL_TYPE, 2> noise() { return m_pedestal.std(); }

    /** @brief Clear the pedestal and mark it as not ready. */
    void clear_pedestal() { m_pedestal.clear(); }

    /** @brief Recompute the threshold as noise multiplied by nSigma. */
    void update_threshold() { m_threshold = m_pedestal.std() * m_nSigma; }

    /**
     * @brief Move out all accumulated clusters and reset the internal vector.
     * @param realloc_same_capacity Preserve the previous capacity when true.
     * @return The accumulated clusters and their frame metadata.
     */
    ClusterVector<ClusterType>
    steal_clusters(bool realloc_same_capacity = false) {
        ClusterVector<ClusterType> tmp = std::move(m_clusters);
        if (realloc_same_capacity)
            m_clusters = ClusterVector<ClusterType>(tmp.capacity());
        else
            m_clusters = ClusterVector<ClusterType>{};
        return tmp;
    }

  private:
    /**
     * @brief Process a single pixel: scan its cluster window, decide whether it
     * is a photon or a pedestal value, and store the cluster if needed.
     * @tparam CheckBounds Skip out-of-image neighbours when true; assume the
     * complete window is in bounds when false. Skipped cluster values remain 0.
     */
    template <bool CheckBounds>
    void process_pixel(const NDView<FRAME_TYPE, 2> &frame, const int iy,
                       const int ix) {

        constexpr int dy = ClusterSizeY / 2;
        constexpr int dx = ClusterSizeX / 2;
        constexpr int has_center_pixel_x = ClusterSizeX % 2;
        constexpr int has_center_pixel_y = ClusterSizeY % 2;

        PEDESTAL_TYPE max = std::numeric_limits<PEDESTAL_TYPE>::lowest();
        PEDESTAL_TYPE total = 0;

        const int cols = static_cast<int>(frame.shape(1));
        const int rows = static_cast<int>(frame.shape(0));
        const auto center =
            (static_cast<std::size_t>(iy) * static_cast<std::size_t>(cols)) +
            static_cast<std::size_t>(ix);
        const auto *corrected = m_pd_corrected_frame.data();
        const PEDESTAL_TYPE threshold = m_threshold.data()[center];
        const PEDESTAL_TYPE value = corrected[center];

        if (value < -threshold)
            return; // NEGATIVE_PEDESTAL, nothing to do for this pixel
                    // TODO! No pedestal update???

        for (int ir = -dy; ir < dy + has_center_pixel_y; ir++) {
            const int y = iy + ir;
            if constexpr (CheckBounds) {
                if (y < 0 || y >= rows)
                    continue;
            }

            const auto *row = corrected + static_cast<std::size_t>(y) * cols;
            for (int ic = -dx; ic < dx + has_center_pixel_x; ic++) {
                const int x = ix + ic;
                if constexpr (CheckBounds) {
                    if (x < 0 || x >= cols)
                        continue;
                }

                const PEDESTAL_TYPE val = row[x];
                total += val;
                max = std::max(max, val);
            }
        }

        if ((max > threshold)) {
            if (value < max)
                return; // Not max go to the next pixel, no pedestal update
        } else if (total > c3 * threshold) {
            // pass, store the cluster below
        } else {
            m_pedestal.push_fast(center, frame.data()[center]);
            return; // It was a pedestal value nothing to store
        }

        // Store cluster
        if (value == max) {
            ClusterType cluster{};
            cluster.x = ix;
            cluster.y = iy;

            int i = 0;
            for (int ir = -dy; ir < dy + has_center_pixel_y; ir++) {
                const int y = iy + ir;
                for (int ic = -dx; ic < dx + has_center_pixel_x; ic++, i++) {
                    const int x = ix + ic;
                    if constexpr (CheckBounds) {
                        if (x < 0 || x >= cols || y < 0 || y >= rows)
                            continue;
                    }

                    const PEDESTAL_TYPE corrected_value =
                        corrected[(static_cast<std::size_t>(y) * cols) + x];
                    // If the cluster type is an integral type, and the
                    // pedestal is a floating point type then we need to
                    // round the value before storing it
                    if constexpr (std::is_integral_v<CT> &&
                                  std::is_floating_point_v<PEDESTAL_TYPE>) {
                        cluster.data[i] =
                            static_cast<CT>(std::lround(corrected_value));
                    }
                    // On the other hand if both are floating point or both
                    // are integral then we can just static cast directly
                    else {
                        cluster.data[i] = static_cast<CT>(corrected_value);
                    }
                }
            }

            // Add the cluster to the output ClusterVector
            m_clusters.push_back(cluster);
        }
    }

  public:
    /**
     * @brief Find clusters in one frame and update eligible pedestal pixels.
     * @param frame Input frame matching the configured image shape.
     * @param frame_number Metadata assigned to the accumulated clusters.
     * @pre frame has the same shape as image_size passed to the constructor.
     * @throws std::runtime_error if the pedestal is not ready.
     * @note Clusters accumulate until steal_clusters() is called.
     */
    void find_clusters(NDView<FRAME_TYPE, 2> frame, uint64_t frame_number = 0) {

        // // TODO! deal with even size clusters
        // // currently 3,3 -> +/- 1
        // //  4,4 -> +/- 2
        if (!m_pedestal.ready()) {
            throw std::runtime_error(
                "Pedestal is not ready, cannot find clusters");
        }

        constexpr int dy = ClusterSizeY / 2;
        constexpr int dx = ClusterSizeX / 2;
        constexpr int has_center_pixel_x = ClusterSizeX % 2;
        constexpr int has_center_pixel_y = ClusterSizeY % 2;

        // Largest neighbour offset below/right of the current pixel. Pixels
        // further than this from an edge have their whole window in bounds.
        constexpr int down = dy + has_center_pixel_y - 1;
        constexpr int right = dx + has_center_pixel_x - 1;

        m_clusters.set_frame_number(frame_number);

        const int rows = static_cast<int>(frame.shape(0));
        const int cols = static_cast<int>(frame.shape(1));

        // TODO! See if we can get the same performace using the operator-
        // m_pd_corrected_frame = frame - m_pedestal.view();

        // here we should be able to safely assume that the frame and corrected
        // frame have the same size
        auto n_pixels = frame.size();
        auto pd = m_pedestal.view().data();
        auto corrected = m_pd_corrected_frame.data();
        auto frame_data = frame.data();
        for (ssize_t i = 0; i < n_pixels; i++) {
            corrected[i] = static_cast<PEDESTAL_TYPE>(frame_data[i]) - pd[i];
        }

        // Interior pixels can skip the per-neighbour bounds checks; pixels
        // within dx/dy of an edge take the bounds-checked path. Iteration order
        // (row-major, increasing ix) is preserved so results are identical.
        const int ix_begin = dx;
        const int ix_end = cols - right; // exclusive

        for (int iy = 0; iy < rows; iy++) {
            const bool interior_row = iy >= dy && iy < rows - down;

            if (!interior_row || ix_begin >= ix_end) {
                for (int ix = 0; ix < cols; ix++)
                    process_pixel<true>(frame, iy, ix);
                continue;
            }

            for (int ix = 0; ix < ix_begin; ix++)
                process_pixel<true>(frame, iy, ix);
            for (int ix = ix_begin; ix < ix_end; ix++)
                process_pixel<false>(frame, iy, ix);
            for (int ix = ix_end; ix < cols; ix++)
                process_pixel<true>(frame, iy, ix);
        }
    }
};

} // namespace aare
