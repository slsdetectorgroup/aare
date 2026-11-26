// SPDX-License-Identifier: MPL-2.0
#include "aare/ClusterConverter.hpp"
#include "aare/GainMap.hpp"
#include "aare/NDArray.hpp"

#include <algorithm>
#include <array>
#include <catch2/catch_all.hpp>
#include <cmath>
#include <cstdint>
#include <random>
#include <type_traits>
#include <utility>

using aare::Cluster;
using aare::ClusterConverter;
using aare::ClusterVector;
using aare::Quantization;
using aare::ssize_t;

namespace {

using IntCluster = Cluster<int32_t, 3, 3>;

// Every pixel of every cluster holds the same value.
ClusterVector<IntCluster> constant_clusters(int32_t value, size_t n,
                                            int32_t frame_number = 0) {
    ClusterVector<IntCluster> clusters(n, frame_number);
    for (size_t i = 0; i < n; ++i) {
        IntCluster cluster{};
        cluster.x = static_cast<uint16_t>(i);
        cluster.y = static_cast<uint16_t>(2 * i);
        cluster.data.fill(value);
        clusters.push_back(cluster);
    }
    return clusters;
}

// Clusters with different pixel values at centers inside a 12 x 12 map.
ClusterVector<IntCluster> varied_clusters(size_t n) {
    ClusterVector<IntCluster> clusters(n);
    for (size_t i = 0; i < n; ++i) {
        IntCluster cluster{};
        cluster.x = static_cast<uint16_t>(1 + i % 10);
        cluster.y = static_cast<uint16_t>(1 + (i / 10) % 10);
        for (size_t j = 0; j < cluster.data.size(); ++j)
            cluster.data[j] = static_cast<int32_t>(i * 7 + j) - 20;
        clusters.push_back(cluster);
    }
    return clusters;
}

struct Range {
    double min;
    double max;
    double mean;
};

template <typename ClusterType>
Range pixel_range(const ClusterVector<ClusterType> &clusters) {
    Range range{1e300, -1e300, 0.0};
    size_t n = 0;
    for (const auto &cluster : clusters) {
        for (const auto value : cluster.data) {
            const auto v = static_cast<double>(value);
            range.min = std::min(range.min, v);
            range.max = std::max(range.max, v);
            range.mean += v;
            ++n;
        }
    }
    range.mean /= static_cast<double>(n);
    return range;
}

} // namespace

TEST_CASE("ClusterConverter returns float clusters by default",
          "[ClusterConverter]") {
    const auto clusters = constant_clusters(7, 4, 123);
    ClusterConverter converter(Quantization::Rounded, 1);
    auto result = converter(clusters);

    STATIC_REQUIRE(
        std::is_same_v<decltype(result), ClusterVector<Cluster<float, 3, 3>>>);
    STATIC_REQUIRE(std::is_same_v<decltype(converter.apply(clusters)),
                                  ClusterVector<Cluster<float, 3, 3>>>);
    REQUIRE(result.size() == clusters.size());
    CHECK(result.frame_number() == 123);
    for (size_t i = 0; i < clusters.size(); ++i) {
        CHECK(result[i].x == clusters[i].x);
        CHECK(result[i].y == clusters[i].y);
    }
}

TEST_CASE("ClusterConverter spreads rounded values around the integer",
          "[ClusterConverter]") {
    const auto value = GENERATE(-40, -1, 0, 1, 40);
    const auto clusters = constant_clusters(value, 20000);
    ClusterConverter converter(Quantization::Rounded, 2);
    const auto range = pixel_range(converter.apply<double>(clusters));

    CHECK(range.min >= value - 0.5);
    CHECK(range.max < value + 0.5);
    CHECK(range.max - range.min > 0.99);
    CHECK(std::abs(range.mean - value) < 0.01);
}

TEST_CASE("ClusterConverter spreads truncated values away from zero",
          "[ClusterConverter]") {
    ClusterConverter converter(Quantization::Truncated, 3);
    SECTION("positive values cover [n, n + 1)") {
        const auto range =
            pixel_range(converter.apply<double>(constant_clusters(40, 20000)));
        CHECK(range.min >= 40.0);
        CHECK(range.max < 41.0);
        CHECK(std::abs(range.mean - 40.5) < 0.01);
    }
    SECTION("negative values cover (n - 1, n]") {
        const auto range =
            pixel_range(converter.apply<double>(constant_clusters(-40, 20000)));
        CHECK(range.min > -41.0);
        CHECK(range.max <= -40.0);
        CHECK(std::abs(range.mean + 40.5) < 0.01);
    }
    SECTION("zero covers (-1, 1)") {
        const auto range =
            pixel_range(converter.apply<double>(constant_clusters(0, 20000)));
        CHECK(range.min > -1.0);
        CHECK(range.min < -0.99);
        CHECK(range.max < 1.0);
        CHECK(range.max > 0.99);
        CHECK(std::abs(range.mean) < 0.01);
    }
}

TEST_CASE("ClusterConverter removes the double zero of truncated data",
          "[ClusterConverter]") {
    // Truncate uniformly distributed values toward zero as a legacy writer
    // did, then check that the result is uniform again.
    std::mt19937_64 source(4);
    std::uniform_real_distribution<double> uniform(-8.0, 8.0);
    const size_t n = 40000;
    ClusterVector<IntCluster> clusters(n);
    for (size_t i = 0; i < n; ++i) {
        IntCluster cluster{};
        for (auto &value : cluster.data)
            value = static_cast<int32_t>(uniform(source));
        clusters.push_back(cluster);
    }

    ClusterConverter converter(Quantization::Truncated, 5);
    const auto result = converter(clusters);
    std::array<size_t, 16> counts{};
    for (const auto &cluster : result) {
        for (const auto value : cluster.data) {
            const auto bin = static_cast<size_t>(std::floor(value) + 8);
            ++counts.at(bin);
        }
    }
    const auto expected = static_cast<double>(n * 9) / 16.0;
    for (const auto count : counts)
        CHECK(std::abs(static_cast<double>(count) - expected) <
              0.03 * expected);
}

TEST_CASE("ClusterConverter is reproducible with a seed",
          "[ClusterConverter]") {
    const auto clusters = constant_clusters(3, 50);
    ClusterConverter first_converter(Quantization::Rounded, 6);
    ClusterConverter same_converter(Quantization::Rounded, 6);
    ClusterConverter other_converter(Quantization::Rounded, 7);
    const auto first = first_converter(clusters);
    const auto same = same_converter(clusters);
    const auto other = other_converter(clusters);
    // The generator moves on: a second call adds different values.
    const auto second = first_converter(clusters);

    bool other_differs = false;
    bool second_differs = false;
    for (size_t i = 0; i < clusters.size(); ++i) {
        CHECK(first[i].data == same[i].data);
        other_differs = other_differs || first[i].data != other[i].data;
        second_differs = second_differs || first[i].data != second[i].data;
    }
    CHECK(other_differs);
    CHECK(second_differs);
}

TEST_CASE("ClusterConverter result does not depend on the chunk size",
          "[ClusterConverter]") {
    // 7 clusters of 9 pixels leave a partly used 64 bit draw between calls.
    const auto clusters = varied_clusters(100);
    const size_t chunk_size = 7;

    ClusterConverter whole_converter(Quantization::Rounded, 14);
    ClusterConverter chunk_converter(Quantization::Rounded, 14);
    const auto whole = whole_converter(clusters);

    size_t compared = 0;
    for (size_t first = 0; first < clusters.size(); first += chunk_size) {
        const auto last = std::min(first + chunk_size, clusters.size());
        ClusterVector<IntCluster> chunk(last - first);
        for (size_t i = first; i < last; ++i)
            chunk.push_back(clusters[i]);
        const auto converted = chunk_converter(chunk);
        REQUIRE(converted.size() == last - first);
        for (size_t i = 0; i < converted.size(); ++i, ++compared)
            CHECK(converted[i].data == whole[compared].data);
    }
    CHECK(compared == clusters.size());
}

TEST_CASE("ClusterConverter reuses the output vector", "[ClusterConverter]") {
    ClusterConverter converter(Quantization::Rounded, 8);
    ClusterVector<Cluster<float, 3, 3>> out(100);
    converter.apply(constant_clusters(5, 100, 1), out);
    const auto *storage = out.data();
    const auto first_pixel = out[0].data[0];

    converter.apply(constant_clusters(9, 60, 2), out);
    CHECK(out.data() == storage);
    CHECK(out.size() == 60);
    CHECK(out.frame_number() == 2);
    CHECK(out[0].data[0] != first_pixel);
    CHECK(out[59].data[8] >= 8.5f);
}

TEST_CASE("ClusterConverter handles other cluster types",
          "[ClusterConverter]") {
    ClusterVector<Cluster<int16_t, 2, 2>> clusters(1);
    clusters.push_back(Cluster<int16_t, 2, 2>{4, 5, {-3, 0, 3, 100}});
    ClusterConverter converter(Quantization::Truncated, 9);
    const auto result = converter.apply<double>(clusters);

    STATIC_REQUIRE(std::is_same_v<decltype(result),
                                  const ClusterVector<Cluster<double, 2, 2>>>);
    CHECK(result[0].x == 4);
    CHECK(result[0].y == 5);
    CHECK(static_cast<int>(result[0].data[0]) == -3);
    CHECK(static_cast<int>(result[0].data[1]) == 0);
    CHECK(static_cast<int>(result[0].data[2]) == 3);
    CHECK(static_cast<int>(result[0].data[3]) == 100);

    ClusterVector<Cluster<int16_t, 2, 2>> empty(0, 17);
    const auto none = converter(empty);
    CHECK(none.empty());
    CHECK(none.frame_number() == 17);
}

TEST_CASE("ClusterConverter rounds and is seeded from the system by default",
          "[ClusterConverter]") {
    ClusterConverter converter;
    const auto range = pixel_range(converter(constant_clusters(40, 2000)));
    CHECK(range.min >= 39.5);
    CHECK(range.max <= 40.5);
    CHECK(range.min < 39.6);
}

TEST_CASE("ClusterConverter applies a uniform gain in the same pass",
          "[ClusterConverter]") {
    const auto clusters = constant_clusters(40, 500, 11);
    ClusterConverter plain_converter(Quantization::Truncated, 10);
    ClusterConverter gain_converter(2.5, Quantization::Truncated, 10);
    const auto plain = plain_converter.apply<double>(clusters);
    const auto scaled = gain_converter.apply<double>(clusters);

    REQUIRE(scaled.size() == plain.size());
    CHECK(scaled.frame_number() == 11);
    for (size_t i = 0; i < plain.size(); ++i) {
        CHECK(scaled[i].x == plain[i].x);
        for (size_t j = 0; j < 9; ++j)
            CHECK(scaled[i].data[j] ==
                  Catch::Approx(plain[i].data[j] / 2.5).epsilon(1e-12));
    }
}

TEST_CASE("ClusterConverter applies a gain map in the same pass",
          "[ClusterConverter]") {
    // The gain of pixel (y, x) is 1 + y + 10 * x, so every pixel of a
    // cluster identifies the map element it was divided by.
    aare::NDArray<double, 2> gain({6, 8});
    for (ssize_t y = 0; y < 6; ++y)
        for (ssize_t x = 0; x < 8; ++x)
            gain(y, x) = 1.0 + static_cast<double>(y + 10 * x);

    ClusterVector<IntCluster> clusters(4);
    IntCluster cluster{};
    cluster.data.fill(1000);
    for (const auto [x, y] :
         {std::pair{3, 2}, std::pair{1, 1}, std::pair{6, 4}, std::pair{0, 2},
          std::pair{7, 2}, std::pair{3, 0}, std::pair{3, 5}}) {
        cluster.x = static_cast<uint16_t>(x);
        cluster.y = static_cast<uint16_t>(y);
        clusters.push_back(cluster);
    }

    ClusterConverter converter(gain.view(), Quantization::Rounded, 12);
    const auto out = converter.apply<double>(clusters);

    REQUIRE(out.size() == 7);
    for (size_t i = 0; i < 3; ++i) {
        for (ssize_t row = 0; row < 3; ++row) {
            for (ssize_t col = 0; col < 3; ++col) {
                const auto y = out[i].y + row - 1;
                const auto x = out[i].x + col - 1;
                const auto value =
                    out[i].data[static_cast<size_t>(row * 3 + col)];
                CHECK(std::abs(value * gain(y, x) - 1000.0) <= 0.5);
            }
        }
    }
    // Clusters reaching beyond the map are kept and cleared.
    for (size_t i = 3; i < 7; ++i) {
        CHECK(out[i].x == clusters[i].x);
        for (const auto value : out[i].data)
            CHECK(value == 0.0);
    }
}

TEST_CASE("ClusterConverter gain variants agree for the same seed",
          "[ClusterConverter]") {
    const auto clusters = varied_clusters(100);
    aare::NDArray<double, 2> gain({12, 12}, 4.0);
    const aare::InvertedGainMap inverse(gain);
    CHECK(inverse(2, 3) == Catch::Approx(0.25));

    ClusterConverter plain_converter(Quantization::Rounded, 15);
    ClusterConverter uniform_converter(4.0, Quantization::Rounded, 15);
    ClusterConverter view_converter(gain.view(), Quantization::Rounded, 15);
    ClusterConverter map_converter(inverse, Quantization::Rounded, 15);
    const auto &const_gain = gain;
    ClusterConverter const_view_converter(const_gain.view(),
                                          Quantization::Rounded, 15);
    const auto plain = plain_converter.apply<double>(clusters);
    const auto uniform = uniform_converter.apply<double>(clusters);
    const auto from_view = view_converter.apply<double>(clusters);
    const auto from_map = map_converter.apply<double>(clusters);
    const auto from_const_view = const_view_converter.apply<double>(clusters);

    for (size_t i = 0; i < clusters.size(); ++i) {
        for (size_t j = 0; j < 9; ++j) {
            CHECK(uniform[i].data[j] == plain[i].data[j] / 4.0);
            CHECK(from_view[i].data[j] == uniform[i].data[j]);
            CHECK(from_const_view[i].data[j] == uniform[i].data[j]);
            CHECK(from_map[i].data[j] == uniform[i].data[j]);
        }
    }
}

TEST_CASE("ClusterConverter with a gain map handles 2x2 clusters",
          "[ClusterConverter]") {
    aare::NDArray<double, 2> gain({4, 4});
    for (ssize_t y = 0; y < 4; ++y)
        for (ssize_t x = 0; x < 4; ++x)
            gain(y, x) = 1.0 + static_cast<double>(y + 10 * x);

    // A 2x2 cluster covers the center and the pixels to its left and top.
    ClusterVector<Cluster<int32_t, 2, 2>> clusters(2);
    clusters.push_back(Cluster<int32_t, 2, 2>{2, 1, {500, 500, 500, 500}});
    clusters.push_back(Cluster<int32_t, 2, 2>{0, 1, {500, 500, 500, 500}});

    ClusterConverter converter(gain.view(), Quantization::Rounded, 13);
    const auto out = converter.apply<double>(clusters);

    CHECK(std::abs(out[0].data[0] * gain(0, 1) - 500.0) <= 0.5);
    CHECK(std::abs(out[0].data[1] * gain(0, 2) - 500.0) <= 0.5);
    CHECK(std::abs(out[0].data[2] * gain(1, 1) - 500.0) <= 0.5);
    CHECK(std::abs(out[0].data[3] * gain(1, 2) - 500.0) <= 0.5);
    for (const auto value : out[1].data)
        CHECK(value == 0.0);
}
