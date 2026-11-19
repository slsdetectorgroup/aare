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

#include <cstdint>
#include <optional>
#include <random>
#include <type_traits>

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

/// Hands out a 64 bit draw in three pieces of 21 bits.
class RandomBits21 {
    std::mt19937_64 &m_generator;
    uint64_t m_bits{};
    int m_left{};

  public:
    explicit RandomBits21(std::mt19937_64 &generator)
        : m_generator(generator) {}

    uint32_t next() {
        if (m_left == 0) {
            m_bits = m_generator();
            m_left = 3;
        }
        const auto value = static_cast<uint32_t>(
            m_bits & ((uint64_t{1} << dequantize_bits) - 1));
        m_bits >>= dequantize_bits;
        --m_left;
        return value;
    }
};

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

inline std::mt19937_64 dequantize_generator(std::optional<uint64_t> seed) {
    if (seed)
        return std::mt19937_64(*seed);
    std::random_device device;
    return std::mt19937_64((static_cast<uint64_t>(device()) << 32) ^ device());
}

/// The same gain for every pixel.
struct UniformInverseGain {
    double inverse_gain;
    constexpr bool covers(ssize_t, ssize_t, ssize_t, ssize_t) const {
        return true;
    }
    constexpr double operator()(ssize_t, ssize_t) const { return inverse_gain; }
};

/// One gain per detector pixel.
struct MappedInverseGain {
    const InvertedGainMap &map;
    bool covers(ssize_t x_first, ssize_t x_last, ssize_t y_first,
                ssize_t y_last) const {
        return x_first >= 0 && y_first >= 0 && x_last < map.cols() &&
               y_last < map.rows();
    }
    double operator()(ssize_t y, ssize_t x) const { return map(y, x); }
};

/**
 * @brief Spread the pixel values and multiply them by the inverted gain of
 * their pixel in one pass.
 */
template <typename OutT, typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType, typename InverseGain>
void dequantize_with_gain(
    const ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>>
        &clusters,
    ClusterVector<Cluster<OutT, ClusterSizeX, ClusterSizeY, CoordType>> &out,
    const Quantization quantization, std::mt19937_64 &generator,
    const InverseGain &inverse_gain) {
    static_assert(std::is_integral_v<T>,
                  "dequantize needs clusters with an integer pixel type");
    static_assert(std::is_floating_point_v<OutT>,
                  "dequantize writes clusters with a floating point type");

    // Position of the pixels relative to the cluster center, as in
    // InvertedGainMap::apply_gain_map.
    constexpr ssize_t size_x = ClusterSizeX;
    constexpr ssize_t size_y = ClusterSizeY;
    constexpr ssize_t left = size_x / 2;
    constexpr ssize_t top = size_y / 2;
    RandomBits21 random(generator);

    out.resize(clusters.size());
    out.set_frame_number(clusters.frame_number());
    for (size_t i = 0; i < clusters.size(); ++i) {
        const auto &source = clusters[i];
        auto &destination = out[i];
        destination.x = source.x;
        destination.y = source.y;

        const auto x_first = static_cast<ssize_t>(source.x) - left;
        const auto y_first = static_cast<ssize_t>(source.y) - top;
        if (!inverse_gain.covers(x_first, x_first + size_x - 1, y_first,
                                 y_first + size_y - 1)) {
            // As apply_gain_map: clear clusters that extend beyond the map.
            destination.data.fill(0);
            continue;
        }
        size_t j = 0;
        for (ssize_t row = 0; row < size_y; ++row) {
            for (ssize_t col = 0; col < size_x; ++col, ++j) {
                const auto value = dequantize_value<double>(
                    source.data[j], random.next(), quantization);
                destination.data[j] = static_cast<OutT>(
                    value * inverse_gain(y_first + row, x_first + col));
            }
        }
    }
}

} // namespace detail

/**
 * @brief Convert integer clusters to floating point, replacing every pixel
 * value with a uniformly distributed value from the interval that was stored
 * as that integer.
 *
 * Integer pixel values make ratios such as eta take only rational values,
 * which shows as spikes in finely binned eta histograms and as lines in
 * interpolated images. Spreading the values removes this without changing
 * their distribution on scales larger than one integer step.
 *
 * Coordinates and the frame number are copied. The previous content of out is
 * replaced and its allocation is reused when it is large enough.
 *
 * @tparam OutT floating point pixel type of the output. float holds integers
 * up to about 2^23 with a fractional part; use double for larger values.
 * @param clusters integer clusters to convert
 * @param out receives the converted clusters
 * @param quantization how the pixel values were converted to integers
 * @param generator source of the random numbers
 */
template <typename OutT, typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType>
void dequantize(
    const ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>>
        &clusters,
    ClusterVector<Cluster<OutT, ClusterSizeX, ClusterSizeY, CoordType>> &out,
    const Quantization quantization, std::mt19937_64 &generator) {
    static_assert(std::is_integral_v<T>,
                  "dequantize needs clusters with an integer pixel type");
    static_assert(std::is_floating_point_v<OutT>,
                  "dequantize writes clusters with a floating point type");

    constexpr size_t n_pixels = size_t{ClusterSizeX} * size_t{ClusterSizeY};
    detail::RandomBits21 random(generator);

    out.resize(clusters.size());
    out.set_frame_number(clusters.frame_number());
    for (size_t i = 0; i < clusters.size(); ++i) {
        const auto &source = clusters[i];
        auto &destination = out[i];
        destination.x = source.x;
        destination.y = source.y;
        for (size_t j = 0; j < n_pixels; ++j) {
            destination.data[j] = detail::dequantize_value<OutT>(
                source.data[j], random.next(), quantization);
        }
    }
}

/**
 * @brief Convert integer clusters to floating point and apply a gain map in
 * one pass.
 *
 * Every pixel value is first spread over the interval that was stored as that
 * integer and then multiplied by the inverted gain of its pixel. Clusters that
 * extend beyond the gain map are kept with all pixel values set to zero, as
 * InvertedGainMap::apply_gain_map does.
 *
 * @param clusters integer clusters to convert
 * @param out receives the converted clusters; its allocation is reused when it
 * is large enough
 * @param quantization how the pixel values were converted to integers
 * @param generator source of the random numbers
 * @param gain_map inverted gain per detector pixel, indexed as [y, x]. Build
 * it once and reuse it for every chunk of a file.
 */
template <typename OutT, typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType>
void dequantize(
    const ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>>
        &clusters,
    ClusterVector<Cluster<OutT, ClusterSizeX, ClusterSizeY, CoordType>> &out,
    const Quantization quantization, std::mt19937_64 &generator,
    const InvertedGainMap &gain_map) {
    detail::dequantize_with_gain(clusters, out, quantization, generator,
                                 detail::MappedInverseGain{gain_map});
}

/**
 * @brief Convert integer clusters to floating point and divide by a gain that
 * is the same for every pixel, in one pass.
 * @param clusters integer clusters to convert
 * @param out receives the converted clusters; its allocation is reused when it
 * is large enough
 * @param quantization how the pixel values were converted to integers
 * @param generator source of the random numbers
 * @param gain gain in ADU/energy
 */
template <typename OutT, typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType>
void dequantize(
    const ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>>
        &clusters,
    ClusterVector<Cluster<OutT, ClusterSizeX, ClusterSizeY, CoordType>> &out,
    const Quantization quantization, std::mt19937_64 &generator,
    const double gain) {
    detail::dequantize_with_gain(clusters, out, quantization, generator,
                                 detail::UniformInverseGain{1.0 / gain});
}

/**
 * @brief Convert integer clusters to floating point, replacing every pixel
 * value with a uniformly distributed value from the interval that was stored
 * as that integer.
 * @tparam OutT floating point pixel type of the result, float by default
 * @param clusters integer clusters to convert
 * @param quantization how the pixel values were converted to integers
 * @param generator source of the random numbers
 * @return converted clusters with the coordinates and frame number of the
 * input
 */
template <typename OutT = float, typename T, uint8_t ClusterSizeX,
          uint8_t ClusterSizeY, typename CoordType>
ClusterVector<Cluster<OutT, ClusterSizeX, ClusterSizeY, CoordType>> dequantize(
    const ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>>
        &clusters,
    const Quantization quantization, std::mt19937_64 &generator) {
    ClusterVector<Cluster<OutT, ClusterSizeX, ClusterSizeY, CoordType>> out(
        clusters.size(), clusters.frame_number());
    dequantize(clusters, out, quantization, generator);
    return out;
}

/**
 * @brief Convert integer clusters to floating point with a generator created
 * for this call.
 * @tparam OutT floating point pixel type of the result, float by default
 * @param clusters integer clusters to convert
 * @param quantization how the pixel values were converted to integers,
 * rounded by default
 * @param seed seed of the generator. Unset means it is seeded from the
 * system. Calls with the same seed add the same sequence, so pass a generator
 * or a different seed per call when converting data in chunks.
 * @return converted clusters with the coordinates and frame number of the
 * input
 */
template <typename OutT = float, typename T, uint8_t ClusterSizeX,
          uint8_t ClusterSizeY, typename CoordType>
ClusterVector<Cluster<OutT, ClusterSizeX, ClusterSizeY, CoordType>> dequantize(
    const ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>>
        &clusters,
    const Quantization quantization = Quantization::Rounded,
    std::optional<uint64_t> seed = {}) {
    auto generator = detail::dequantize_generator(seed);
    return dequantize<OutT>(clusters, quantization, generator);
}

} // namespace aare
