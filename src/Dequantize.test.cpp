// SPDX-License-Identifier: MPL-2.0
#include "aare/Dequantize.hpp"
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
using aare::ClusterVector;
using aare::dequantize;
using aare::Quantization;

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

TEST_CASE("dequantize returns float clusters by default", "[dequantize]") {
    const auto clusters = constant_clusters(7, 4, 123);
    auto result = dequantize(clusters, Quantization::Rounded, 1);

    STATIC_REQUIRE(
        std::is_same_v<decltype(result), ClusterVector<Cluster<float, 3, 3>>>);
    REQUIRE(result.size() == clusters.size());
    CHECK(result.frame_number() == 123);
    for (size_t i = 0; i < clusters.size(); ++i) {
        CHECK(result[i].x == clusters[i].x);
        CHECK(result[i].y == clusters[i].y);
    }
}

TEST_CASE("dequantize spreads rounded values around the integer",
          "[dequantize]") {
    const auto value = GENERATE(-40, -1, 0, 1, 40);
    const auto clusters = constant_clusters(value, 20000);
    const auto range =
        pixel_range(dequantize<double>(clusters, Quantization::Rounded, 2));

    CHECK(range.min >= value - 0.5);
    CHECK(range.max < value + 0.5);
    CHECK(range.max - range.min > 0.99);
    CHECK(std::abs(range.mean - value) < 0.01);
}

TEST_CASE("dequantize spreads truncated values away from zero",
          "[dequantize]") {
    SECTION("positive values cover [n, n + 1)") {
        const auto range = pixel_range(dequantize<double>(
            constant_clusters(40, 20000), Quantization::Truncated, 3));
        CHECK(range.min >= 40.0);
        CHECK(range.max < 41.0);
        CHECK(std::abs(range.mean - 40.5) < 0.01);
    }
    SECTION("negative values cover (n - 1, n]") {
        const auto range = pixel_range(dequantize<double>(
            constant_clusters(-40, 20000), Quantization::Truncated, 3));
        CHECK(range.min > -41.0);
        CHECK(range.max <= -40.0);
        CHECK(std::abs(range.mean + 40.5) < 0.01);
    }
    SECTION("zero covers (-1, 1)") {
        const auto range = pixel_range(dequantize<double>(
            constant_clusters(0, 20000), Quantization::Truncated, 3));
        CHECK(range.min > -1.0);
        CHECK(range.min < -0.99);
        CHECK(range.max < 1.0);
        CHECK(range.max > 0.99);
        CHECK(std::abs(range.mean) < 0.01);
    }
}

TEST_CASE("dequantize removes the double zero of truncated data",
          "[dequantize]") {
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

    const auto result = dequantize(clusters, Quantization::Truncated, 5);
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

TEST_CASE("dequantize is reproducible with a seed", "[dequantize]") {
    const auto clusters = constant_clusters(3, 50);
    const auto first = dequantize(clusters, Quantization::Rounded, 6);
    const auto same = dequantize(clusters, Quantization::Rounded, 6);
    const auto other = dequantize(clusters, Quantization::Rounded, 7);

    bool differs = false;
    for (size_t i = 0; i < clusters.size(); ++i) {
        CHECK(first[i].data == same[i].data);
        differs = differs || first[i].data != other[i].data;
    }
    CHECK(differs);
}

TEST_CASE("dequantize reuses the output vector", "[dequantize]") {
    std::mt19937_64 generator(8);
    ClusterVector<Cluster<float, 3, 3>> out(100);
    dequantize(constant_clusters(5, 100, 1), out, Quantization::Rounded,
               generator);
    const auto *storage = out.data();
    const auto first_pixel = out[0].data[0];

    dequantize(constant_clusters(9, 60, 2), out, Quantization::Rounded,
               generator);
    CHECK(out.data() == storage);
    CHECK(out.size() == 60);
    CHECK(out.frame_number() == 2);
    CHECK(out[0].data[0] != first_pixel);
    CHECK(out[59].data[8] >= 8.5f);
}

TEST_CASE("dequantize handles other cluster types", "[dequantize]") {
    ClusterVector<Cluster<int16_t, 2, 2>> clusters(1);
    clusters.push_back(Cluster<int16_t, 2, 2>{4, 5, {-3, 0, 3, 100}});
    const auto result =
        dequantize<double>(clusters, Quantization::Truncated, 9);

    STATIC_REQUIRE(std::is_same_v<decltype(result),
                                  const ClusterVector<Cluster<double, 2, 2>>>);
    CHECK(result[0].x == 4);
    CHECK(result[0].y == 5);
    CHECK(static_cast<int>(result[0].data[0]) == -3);
    CHECK(static_cast<int>(result[0].data[1]) == 0);
    CHECK(static_cast<int>(result[0].data[2]) == 3);
    CHECK(static_cast<int>(result[0].data[3]) == 100);

    ClusterVector<Cluster<int16_t, 2, 2>> empty(0, 17);
    const auto none = dequantize(empty, Quantization::Rounded, 9);
    CHECK(none.empty());
    CHECK(none.frame_number() == 17);
}

TEST_CASE("dequantize rounds by default", "[dequantize]") {
    const auto range = pixel_range(dequantize(constant_clusters(40, 2000)));
    CHECK(range.min >= 39.5);
    CHECK(range.max <= 40.5);
    CHECK(range.min < 39.6);
}

TEST_CASE("dequantize applies a uniform gain in the same pass",
          "[dequantize]") {
    const auto clusters = constant_clusters(40, 500, 11);
    std::mt19937_64 plain_generator(10);
    std::mt19937_64 gain_generator(10);
    ClusterVector<Cluster<double, 3, 3>> plain(0);
    ClusterVector<Cluster<double, 3, 3>> scaled(0);
    dequantize(clusters, plain, Quantization::Truncated, plain_generator);
    dequantize(clusters, scaled, Quantization::Truncated, gain_generator, 2.5);

    REQUIRE(scaled.size() == plain.size());
    CHECK(scaled.frame_number() == 11);
    for (size_t i = 0; i < plain.size(); ++i) {
        CHECK(scaled[i].x == plain[i].x);
        for (size_t j = 0; j < 9; ++j)
            CHECK(scaled[i].data[j] ==
                  Catch::Approx(plain[i].data[j] / 2.5).epsilon(1e-12));
    }
}

TEST_CASE("dequantize applies a gain map in the same pass", "[dequantize]") {
    // The gain of pixel (y, x) is 1 + y + 10 * x, so every pixel of a
    // cluster identifies the map element it was divided by.
    aare::NDArray<double, 2> gain({6, 8});
    for (ssize_t y = 0; y < 6; ++y)
        for (ssize_t x = 0; x < 8; ++x)
            gain(y, x) = 1.0 + static_cast<double>(y + 10 * x);
    const aare::InvertedGainMap inverse(gain);
    CHECK(inverse.rows() == 6);
    CHECK(inverse.cols() == 8);
    CHECK(inverse(2, 3) == Catch::Approx(1.0 / 33.0));

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

    std::mt19937_64 generator(12);
    ClusterVector<Cluster<double, 3, 3>> out(0);
    dequantize(clusters, out, Quantization::Rounded, generator, inverse);

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

TEST_CASE("dequantize with a gain map handles 2x2 clusters", "[dequantize]") {
    aare::NDArray<double, 2> gain({4, 4});
    for (ssize_t y = 0; y < 4; ++y)
        for (ssize_t x = 0; x < 4; ++x)
            gain(y, x) = 1.0 + static_cast<double>(y + 10 * x);
    const aare::InvertedGainMap inverse(gain);

    // A 2x2 cluster covers the center and the pixels to its left and top.
    ClusterVector<Cluster<int32_t, 2, 2>> clusters(2);
    clusters.push_back(Cluster<int32_t, 2, 2>{2, 1, {500, 500, 500, 500}});
    clusters.push_back(Cluster<int32_t, 2, 2>{0, 1, {500, 500, 500, 500}});

    std::mt19937_64 generator(13);
    ClusterVector<Cluster<double, 2, 2>> out(0);
    dequantize(clusters, out, Quantization::Rounded, generator, inverse);

    CHECK(std::abs(out[0].data[0] * gain(0, 1) - 500.0) <= 0.5);
    CHECK(std::abs(out[0].data[1] * gain(0, 2) - 500.0) <= 0.5);
    CHECK(std::abs(out[0].data[2] * gain(1, 1) - 500.0) <= 0.5);
    CHECK(std::abs(out[0].data[3] * gain(1, 2) - 500.0) <= 0.5);
    for (const auto value : out[1].data)
        CHECK(value == 0.0);
}
