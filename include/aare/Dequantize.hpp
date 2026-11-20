// SPDX-License-Identifier: MPL-2.0
/************************************************
 * @file Dequantize.hpp
 * @short convert integer clusters to floating point and spread every pixel
 * value over the interval of analog values that was stored as that integer
 ***********************************************/

#pragma once
#include "aare/Cluster.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/GainMap.hpp"
#include "aare/NDView.hpp"

#include <cstdint>
#include <optional>
#include <random>
#include <type_traits>
#include <utility>

namespace aare {

/**
 * @brief How the writer of the clusters converted pixel values to integers.
 */
enum class Quantization {
    /// Rounded to the nearest integer: n stands for [n - 0.5, n + 0.5).
    /// ClusterFinder writes integer clusters this way.
    Rounded,
    /// Truncated toward zero: n > 0 stands for [n, n + 1), n < 0 for
    /// (n - 1, n] and 0 for (-1, 1). In such data the value 0 occurs about
    /// twice as often as the values next to it.
    Truncated
};

namespace detail {

/// Number of random bits used for one pixel.
constexpr int dequantize_bits = 21;

/**
 * @brief Draw a value from the interval that was stored as the integer value.
 * @param value stored integer
 * @param bits 21 uniformly distributed random bits
 * @param quantization how the integer was produced
 */
template <typename OutT, typename T>
OutT dequantize_value(const T value, const uint32_t bits,
                      const Quantization quantization) {
    constexpr OutT step =
        OutT{1} / static_cast<OutT>(uint32_t{1} << dequantize_bits);
    const auto stored = static_cast<OutT>(value);
    const OutT fraction = static_cast<OutT>(bits) * step; // [0, 1)

    if (quantization == Quantization::Rounded)
        return stored + (fraction - OutT{0.5});
    if (value > 0)
        return stored + fraction;
    if (value < 0)
        return stored - fraction;
    // Zero collects (-1, 1): one bit for the sign, the rest for the magnitude.
    const OutT magnitude = static_cast<OutT>(bits >> 1) * (2 * step);
    return (bits & 1U) ? magnitude : -magnitude;
}

} // namespace detail

/**
 * @brief Converts integer clusters to floating point, replacing every pixel
 * value with a uniformly distributed value from the interval that was stored
 * as that integer, and optionally applies a gain in the same pass.
 *
 * Integer pixel values make ratios such as eta take only rational values,
 * which shows as spikes in finely binned eta histograms and as lines in
 * interpolated images. Spreading the values removes this without changing
 * their distribution on scales larger than one integer step.
 *
 * Create one Dequantizer and use it for all the clusters of a measurement. It
 * keeps the state of its random generator between calls, so the result does
 * not depend on how the clusters are split into chunks. An object must not be
 * used from several threads at the same time; create one per thread, each
 * with its own seed.
 */
class Dequantizer {
  public:
    /**
     * @brief Dequantizer that does not apply a gain.
     * @param quantization how the pixel values were converted to integers
     * @param seed seed of the random generator. Unset means it is seeded from
     * the system.
     */
    explicit Dequantizer(
        const Quantization quantization = Quantization::Rounded,
        const std::optional<uint64_t> seed = {})
        : m_quantization(quantization), m_generator(make_generator(seed)) {}

    /**
     * @brief Dequantizer that divides the spread values by a gain that is the
     * same for every pixel.
     * @param gain gain in ADU/energy
     * @param quantization how the pixel values were converted to integers
     * @param seed seed of the random generator. Unset means it is seeded from
     * the system.
     */
    explicit Dequantizer(
        const double gain,
        const Quantization quantization = Quantization::Rounded,
        const std::optional<uint64_t> seed = {})
        : m_quantization(quantization), m_inverse_gain(1.0 / gain),
          m_generator(make_generator(seed)) {}

    /**
     * @brief Dequantizer that multiplies the spread values by the inverted
     * gain of their pixel.
     *
     * Clusters that extend beyond the gain map are kept with all pixel values
     * set to zero, as InvertedGainMap::apply_gain_map does.
     *
     * @param gain_map inverted gain per detector pixel, indexed as [y, x]
     * @param quantization how the pixel values were converted to integers
     * @param seed seed of the random generator. Unset means it is seeded from
     * the system.
     */
    explicit Dequantizer(
        InvertedGainMap gain_map,
        const Quantization quantization = Quantization::Rounded,
        const std::optional<uint64_t> seed = {})
        : m_quantization(quantization), m_gain_map(std::move(gain_map)),
          m_generator(make_generator(seed)) {}

    /**
     * @brief Dequantizer that divides the spread values by the gain of their
     * pixel.
     * @param gain_map gain in ADU/energy per detector pixel, indexed as
     * [y, x]. The map is copied and inverted.
     * @param quantization how the pixel values were converted to integers
     * @param seed seed of the random generator. Unset means it is seeded from
     * the system.
     */
    explicit Dequantizer(
        const NDView<double, 2> gain_map,
        const Quantization quantization = Quantization::Rounded,
        const std::optional<uint64_t> seed = {})
        : Dequantizer(InvertedGainMap(gain_map), quantization, seed) {}

    /**
     * @brief Convert clusters into an existing vector.
     *
     * Coordinates and the frame number are copied. The previous content of
     * out is replaced and its allocation is reused when it is large enough.
     *
     * @tparam OutT floating point pixel type of the output. float holds
     * integers up to about 2^23 with a fractional part; use double for larger
     * values.
     * @param clusters integer clusters to convert
     * @param out receives the converted clusters
     */
    template <typename OutT, typename T, uint8_t ClusterSizeX,
              uint8_t ClusterSizeY, typename CoordType>
    void dequantize(
        const ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>>
            &clusters,
        ClusterVector<Cluster<OutT, ClusterSizeX, ClusterSizeY, CoordType>>
            &out) {
        static_assert(std::is_integral_v<T>,
                      "Dequantizer needs clusters with an integer pixel type");
        static_assert(std::is_floating_point_v<OutT>,
                      "Dequantizer writes clusters with a floating point type");
        using ClusterType = Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>;

        // Position of the pixels relative to the cluster center, as in
        // InvertedGainMap::apply_gain_map.
        constexpr ssize_t size_x = ClusterSizeX;
        constexpr ssize_t size_y = ClusterSizeY;
        const InvertedGainMap *gain_map = m_gain_map ? &*m_gain_map : nullptr;

        out.resize(clusters.size());
        out.set_frame_number(clusters.frame_number());
        for (size_t i = 0; i < clusters.size(); ++i) {
            const auto &source = clusters[i];
            auto &destination = out[i];
            destination.x = source.x;
            destination.y = source.y;

            const auto center_x = static_cast<ssize_t>(source.x);
            const auto center_y = static_cast<ssize_t>(source.y);
            if (gain_map &&
                !gain_map->covers<ClusterType>(center_x, center_y)) {
                destination.data.fill(0);
                continue;
            }
            const ssize_t x_first = center_x - size_x / 2;
            const ssize_t y_first = center_y - size_y / 2;
            size_t j = 0;
            for (ssize_t row = 0; row < size_y; ++row) {
                for (ssize_t col = 0; col < size_x; ++col, ++j) {
                    const auto value = detail::dequantize_value<double>(
                        source.data[j], next_bits(), m_quantization);
                    const double inverse_gain =
                        gain_map ? (*gain_map)(y_first + row, x_first + col)
                                 : m_inverse_gain;
                    destination.data[j] =
                        static_cast<OutT>(value * inverse_gain);
                }
            }
        }
    }

    /**
     * @brief Convert clusters.
     * @tparam OutT floating point pixel type of the result, float by default
     * @param clusters integer clusters to convert
     * @return converted clusters with the coordinates and frame number of the
     * input
     */
    template <typename OutT = float, typename T, uint8_t ClusterSizeX,
              uint8_t ClusterSizeY, typename CoordType>
    ClusterVector<Cluster<OutT, ClusterSizeX, ClusterSizeY, CoordType>>
    dequantize(
        const ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>>
            &clusters) {
        ClusterVector<Cluster<OutT, ClusterSizeX, ClusterSizeY, CoordType>> out(
            clusters.size(), clusters.frame_number());
        dequantize(clusters, out);
        return out;
    }

    /**
     * @brief Convert clusters to float clusters.
     * @param clusters integer clusters to convert
     * @return converted clusters with the coordinates and frame number of the
     * input
     */
    template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
              typename CoordType>
    ClusterVector<Cluster<float, ClusterSizeX, ClusterSizeY, CoordType>>
    operator()(
        const ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>>
            &clusters) {
        return dequantize<float>(clusters);
    }

  private:
    static std::mt19937_64 make_generator(const std::optional<uint64_t> seed) {
        if (seed)
            return std::mt19937_64(*seed);
        std::random_device device;
        return std::mt19937_64((static_cast<uint64_t>(device()) << 32) ^
                               device());
    }

    /// Hands out a 64 bit draw in three pieces of 21 bits. What is left of a
    /// draw is kept for the next call.
    uint32_t next_bits() {
        if (m_pieces_left == 0) {
            m_bits = m_generator();
            m_pieces_left = 3;
        }
        const auto value = static_cast<uint32_t>(
            m_bits & ((uint64_t{1} << detail::dequantize_bits) - 1));
        m_bits >>= detail::dequantize_bits;
        --m_pieces_left;
        return value;
    }

    Quantization m_quantization;
    std::optional<InvertedGainMap> m_gain_map;
    double m_inverse_gain{1.0}; // used when there is no gain map
    std::mt19937_64 m_generator;
    uint64_t m_bits{};
    int m_pieces_left{};
};

} // namespace aare
