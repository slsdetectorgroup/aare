// SPDX-License-Identifier: MPL-2.0
#include "aare/VarClusterFinder.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cstdlib>
#include <set>
#include <utility>

using namespace aare;
using Finder = VarClusterFinder<double>;
using Hits = Finder::Hits;

namespace {

std::set<std::pair<int, int>> pixels_of(const Hits &hits, size_t i) {
    const auto &hit = hits.hits[i];
    std::set<std::pair<int, int>> pixels;
    for (int64_t k = hit.offset; k < hit.offset + hit.size; ++k)
        pixels.emplace(hits.rows[k], hits.cols[k]);
    return pixels;
}

// Centre value 100 surrounded by 20 in the eight neighbours, clipped at the
// image border.
void add_cluster(NDArray<double, 2> &img, int row, int col) {
    for (int dr = -1; dr <= 1; ++dr)
        for (int dc = -1; dc <= 1; ++dc) {
            const int r = row + dr;
            const int c = col + dc;
            if (r < 0 || c < 0 || r >= img.shape(0) || c >= img.shape(1))
                continue;
            img(r, c) += (dr == 0 && dc == 0) ? 100.0 : 20.0;
        }
}

} // namespace

TEST_CASE("A single 3x3 cluster above a fixed threshold", "[varcluster]") {
    NDArray<double, 2> img({8, 8}, 0.0);
    add_cluster(img, 3, 4);

    Finder finder({8, 8}, 10.0);
    finder.find_clusters(img.view());
    auto hits = finder.steal_hits();

    REQUIRE(hits.size() == 1);
    REQUIRE(hits.n_pixels() == 9);
    const auto &hit = hits.hits[0];
    CHECK(hit.size == 9);
    CHECK(hit.offset == 0);
    CHECK(hit.row == 3);
    CHECK(hit.col == 4);
    CHECK(hit.max == 100.0);
    CHECK_THAT(hit.energy, Catch::Matchers::WithinAbs(260.0, 1e-12));

    // The seed is the first pixel in scan order
    CHECK(hits.rows[0] == 2);
    CHECK(hits.cols[0] == 3);

    std::set<std::pair<int, int>> expected;
    for (int r = 2; r <= 4; ++r)
        for (int c = 3; c <= 5; ++c)
            expected.emplace(r, c);
    CHECK(pixels_of(hits, 0) == expected);

    // Stored pixel values match the stored coordinates
    for (size_t i = 0; i < hits.n_pixels(); ++i) {
        const double expected_value =
            (hits.rows[i] == 3 && hits.cols[i] == 4) ? 100.0 : 20.0;
        CHECK(hits.values[i] == expected_value);
    }
}

TEST_CASE("A cluster in the image corner is clipped", "[varcluster]") {
    NDArray<double, 2> img({8, 8}, 0.0);
    add_cluster(img, 0, 0);

    Finder finder({8, 8}, 10.0);
    finder.find_clusters(img.view());
    auto hits = finder.steal_hits();

    REQUIRE(hits.size() == 1);
    CHECK(hits.hits[0].size == 4);
    CHECK(hits.hits[0].row == 0);
    CHECK(hits.hits[0].col == 0);
    CHECK_THAT(hits.hits[0].energy, Catch::Matchers::WithinAbs(160.0, 1e-12));
}

TEST_CASE("Separated clusters give separate hits in scan order",
          "[varcluster]") {
    NDArray<double, 2> img({8, 8}, 0.0);
    add_cluster(img, 6, 1);
    add_cluster(img, 1, 6);
    add_cluster(img, 1, 1);

    Finder finder({8, 8}, 10.0);
    finder.find_clusters(img.view());
    auto hits = finder.steal_hits();

    REQUIRE(hits.size() == 3);
    CHECK(hits.hits[0].row == 1);
    CHECK(hits.hits[0].col == 1);
    CHECK(hits.hits[1].row == 1);
    CHECK(hits.hits[1].col == 6);
    CHECK(hits.hits[2].row == 6);
    CHECK(hits.hits[2].col == 1);
    for (const auto &hit : hits.hits)
        CHECK(hit.size == 9);

    // The pixel slices are contiguous and in hit order
    CHECK(hits.hits[0].offset == 0);
    CHECK(hits.hits[1].offset == 9);
    CHECK(hits.hits[2].offset == 18);
    CHECK(hits.n_pixels() == 27);
    for (size_t i = 0; i < 3; ++i) {
        const auto pixels = pixels_of(hits, i);
        CHECK(pixels.size() == 9);
        for (const auto &[r, c] : pixels) {
            CHECK(std::abs(r - hits.hits[i].row) <= 1);
            CHECK(std::abs(c - hits.hits[i].col) <= 1);
        }
    }

    auto labels = finder.labeled();
    CHECK(labels(1, 1) == 1);
    CHECK(labels(0, 0) == 1);
    CHECK(labels(2, 7) == 2);
    CHECK(labels(7, 2) == 3);
    CHECK(labels(4, 4) == 0);
    CHECK(labels(0, 3) == 0);
}

TEST_CASE("Diagonal neighbours join with 8 but not with 4 connectivity",
          "[varcluster]") {
    NDArray<double, 2> img({8, 8}, 0.0);
    img(2, 2) = 50.0;
    img(3, 3) = 40.0;

    Finder finder({8, 8}, 10.0);

    SECTION("8 neighbours (default)") {
        CHECK(finder.connectivity() == 8);
        finder.find_clusters(img.view());
        auto hits = finder.steal_hits();
        REQUIRE(hits.size() == 1);
        CHECK(hits.hits[0].size == 2);
        CHECK(hits.hits[0].row == 2);
        CHECK(hits.hits[0].col == 2);
        CHECK(hits.hits[0].max == 50.0);
        CHECK(hits.hits[0].energy == 90.0);
    }
    SECTION("4 neighbours") {
        finder.set_connectivity(4);
        CHECK(finder.connectivity() == 4);
        finder.find_clusters(img.view());
        auto hits = finder.steal_hits();
        REQUIRE(hits.size() == 2);
        CHECK(hits.hits[0].size == 1);
        CHECK(hits.hits[0].energy == 50.0);
        CHECK(hits.hits[1].size == 1);
        CHECK(hits.hits[1].offset == 1);
        CHECK(hits.hits[1].energy == 40.0);
    }
    SECTION("Only 4 and 8 neighbours are accepted") {
        CHECK_THROWS_AS(finder.set_connectivity(6), std::runtime_error);
        CHECK_THROWS_AS(finder.set_connectivity(0), std::runtime_error);
        CHECK(finder.connectivity() == 8);
    }
}

TEST_CASE("Noise map thresholds: seeds and neighbours use their own factors",
          "[varcluster]") {
    NDArray<double, 2> img({8, 8}, 0.0);
    img(4, 4) = 10.0; // seed: above 5 sigma
    img(4, 5) = 3.0;  // neighbour: between 2 and 5 sigma
    img(1, 1) = 4.0;  // isolated, below 5 sigma: never a seed by default
    NDArray<double, 2> noise({8, 8}, 1.0);

    Finder finder({8, 8}, 1000.0); // fixed threshold must not be used
    finder.set_noise_map(noise.view());

    SECTION("Default peripheral factor 5 excludes the 3 sigma neighbour") {
        finder.find_clusters(img.view());
        auto hits = finder.steal_hits();
        REQUIRE(hits.size() == 1);
        CHECK(hits.hits[0].size == 1);
        CHECK(hits.hits[0].energy == 10.0);
    }
    SECTION("Neighbour factor 2 includes the 3 sigma neighbour") {
        finder.set_neighbour_factor(2);
        CHECK(finder.neighbour_factor() == 2);
        CHECK(finder.seed_factor() == 5);
        finder.find_clusters(img.view());
        auto hits = finder.steal_hits();
        REQUIRE(hits.size() == 1);
        CHECK(hits.hits[0].size == 2);
        CHECK(hits.hits[0].energy == 13.0);
        CHECK(hits.hits[0].row == 4);
        CHECK(hits.hits[0].col == 4);
    }
    SECTION("Seed factor 3.5 makes the isolated 4 sigma pixel a seed") {
        finder.set_seed_factor(3.5);
        finder.find_clusters(img.view());
        auto hits = finder.steal_hits();
        REQUIRE(hits.size() == 2);
        CHECK(hits.hits[0].row == 1);
        CHECK(hits.hits[0].col == 1);
        CHECK(hits.hits[0].energy == 4.0);
        CHECK(hits.hits[1].energy == 10.0);
    }
    SECTION("The noise map is copied") {
        REQUIRE(finder.noise_map().has_value());
        CHECK(*finder.noise_map() == noise);
        noise = 1000.0;
        finder.find_clusters(img.view());
        CHECK(finder.total_clusters() == 1);
        CHECK_FALSE(*finder.noise_map() == noise);
    }
    SECTION("clear_noise_map returns to the fixed threshold") {
        finder.find_clusters(img.view());
        CHECK(finder.total_clusters() == 1);
        finder.clear_noise_map();
        finder.find_clusters(img.view()); // fixed threshold 1000: no hits
        CHECK(finder.total_clusters() == 1);
        finder.set_noise_map(noise.view());
        finder.find_clusters(img.view());
        CHECK(finder.total_clusters() == 2);
    }
    SECTION("A noise map of the wrong shape is rejected") {
        NDArray<double, 2> wrong({8, 9}, 1.0);
        CHECK_THROWS_AS(finder.set_noise_map(wrong.view()), std::runtime_error);
    }
}

TEST_CASE("Large clusters store every pixel", "[varcluster]") {
    NDArray<double, 2> img({10, 10}, 100.0);
    img(7, 2) = 300.0;

    Finder finder({10, 10}, 10.0);
    finder.find_clusters(img.view());
    auto hits = finder.steal_hits();

    REQUIRE(hits.size() == 1);
    CHECK(hits.hits[0].size == 100);
    CHECK(hits.n_pixels() == 100);
    CHECK_THAT(hits.hits[0].energy, Catch::Matchers::WithinAbs(10200.0, 1e-9));
    CHECK(hits.hits[0].max == 300.0);
    CHECK(hits.hits[0].row == 7);
    CHECK(hits.hits[0].col == 2);
    CHECK(pixels_of(hits, 0).size() == 100);
}

TEST_CASE("A whole module above threshold is one cluster", "[varcluster]") {
    // Regression test: the recursive fill overflowed the stack here.
    const Shape<2> shape{512, 1024};
    NDArray<double, 2> img(shape, 100.0);

    Finder finder(shape, 10.0);
    finder.find_clusters(img.view());
    auto hits = finder.steal_hits();

    REQUIRE(hits.size() == 1);
    CHECK(hits.hits[0].size == 512 * 1024);
    CHECK(hits.n_pixels() == 512 * 1024);
    CHECK_THAT(hits.hits[0].energy,
               Catch::Matchers::WithinRel(100.0 * 512 * 1024, 1e-12));
    auto labels = finder.labeled();
    CHECK(std::all_of(labels.begin(), labels.end(),
                      [](int32_t l) { return l == 1; }));
}

TEST_CASE("Negative thresholds record the maximum correctly", "[varcluster]") {
    NDArray<double, 2> img({8, 8}, -100.0);
    for (int r = 2; r <= 4; ++r)
        for (int c = 2; c <= 4; ++c)
            img(r, c) = -30.0;
    img(3, 3) = -10.0;

    Finder finder({8, 8}, -50.0);
    finder.find_clusters(img.view());
    auto hits = finder.steal_hits();

    REQUIRE(hits.size() == 1);
    CHECK(hits.hits[0].size == 9);
    CHECK(hits.hits[0].max == -10.0);
    CHECK(hits.hits[0].row == 3);
    CHECK(hits.hits[0].col == 3);
    CHECK_THAT(hits.hits[0].energy, Catch::Matchers::WithinAbs(-250.0, 1e-12));
}

TEST_CASE("Image shape must match the finder", "[varcluster]") {
    Finder finder({8, 8}, 10.0);
    NDArray<double, 2> wrong({8, 9}, 0.0);
    CHECK_THROWS_AS(finder.find_clusters(wrong.view()), std::runtime_error);
    NDArray<double, 2> right({8, 8}, 0.0);
    CHECK_NOTHROW(finder.find_clusters(right.view()));
}

TEST_CASE("Shapes that do not fit 16 bit coordinates are rejected",
          "[varcluster]") {
    CHECK_THROWS_AS(Finder({0, 8}, 10.0), std::runtime_error);
    CHECK_THROWS_AS(Finder({8, 40000}, 10.0), std::runtime_error);
    CHECK_NOTHROW(Finder({1, 32767}, 10.0));
}

TEST_CASE("steal_hits empties the finder and total_clusters counts held hits",
          "[varcluster]") {
    NDArray<double, 2> img({8, 8}, 0.0);
    add_cluster(img, 3, 3);

    Finder finder({8, 8}, 10.0);
    CHECK(finder.total_clusters() == 0);
    finder.find_clusters(img.view());
    CHECK(finder.total_clusters() == 1);
    auto hits = finder.steal_hits();
    CHECK(hits.size() == 1);
    CHECK(finder.total_clusters() == 0);
    auto empty = finder.steal_hits();
    CHECK(empty.empty());
    CHECK(empty.n_pixels() == 0);
}

TEST_CASE("Hits accumulate over frames until cleared", "[varcluster]") {
    NDArray<double, 2> img({8, 8}, 0.0);
    add_cluster(img, 3, 3);

    Finder finder({8, 8}, 10.0);
    finder.find_clusters(img.view());
    finder.find_clusters(img.view());
    CHECK(finder.total_clusters() == 2);

    // The second frame's pixels follow the first frame's in the arrays
    auto hits = finder.steal_hits();
    REQUIRE(hits.size() == 2);
    CHECK(hits.hits[0].offset == 0);
    CHECK(hits.hits[1].offset == 9);
    CHECK(hits.n_pixels() == 18);
    CHECK(pixels_of(hits, 0) == pixels_of(hits, 1));

    finder.find_clusters(img.view());
    finder.clear_hits();
    CHECK(finder.total_clusters() == 0);
    CHECK(finder.steal_hits().n_pixels() == 0);
}

TEST_CASE("The input is left untouched unless erase_clusters is enabled",
          "[varcluster]") {
    NDArray<double, 2> img({8, 8}, 1.0);
    add_cluster(img, 3, 3);
    const NDArray<double, 2> original(img);

    Finder finder({8, 8}, 10.0);

    SECTION("Default: no modification") {
        CHECK_FALSE(finder.erase_clusters());
        finder.find_clusters(img.view());
        CHECK(img == original);
    }
    SECTION("Enabled: cluster pixels and the sub-threshold ring are zeroed") {
        finder.set_erase_clusters(true);
        CHECK(finder.erase_clusters());
        finder.find_clusters(img.view());
        for (int r = 0; r < 8; ++r)
            for (int c = 0; c < 8; ++c) {
                const bool touched = r >= 1 && r <= 5 && c >= 1 && c <= 5;
                CHECK(img(r, c) == (touched ? 0.0 : 1.0));
            }
        CHECK(finder.total_clusters() == 1);
    }
    SECTION("Zeroing does not change the hits of clusters sharing a ring") {
        // The ring of the first cluster is zeroed before the second cluster
        // is filled; with a non-negative threshold that cannot change it.
        NDArray<double, 2> two({8, 12}, 1.0);
        add_cluster(two, 3, 3);
        add_cluster(two, 3, 7);
        NDArray<double, 2> copy(two);

        Finder plain({8, 12}, 10.0);
        plain.find_clusters(two.view());
        Finder zeroing({8, 12}, 10.0);
        zeroing.set_erase_clusters(true);
        zeroing.find_clusters(copy.view());

        const auto a = plain.steal_hits();
        const auto b = zeroing.steal_hits();
        REQUIRE(a.size() == 2);
        REQUIRE(b.size() == 2);
        for (size_t i = 0; i < a.size(); ++i) {
            CHECK(a.hits[i].size == b.hits[i].size);
            CHECK(a.hits[i].energy == b.hits[i].energy);
            CHECK(a.hits[i].row == b.hits[i].row);
            CHECK(a.hits[i].col == b.hits[i].col);
            CHECK(pixels_of(a, i) == pixels_of(b, i));
        }
        CHECK(copy(3, 5) == 0.0);
    }
}

TEST_CASE("Masked pixels never seed and are never added to a cluster",
          "[varcluster]") {
    NDArray<double, 2> img({8, 8}, 0.0);
    add_cluster(img, 3, 4);
    img(7, 0) = 50.0; // hot pixel
    NDArray<bool, 2> mask({8, 8}, false);
    mask(3, 4) = true; // the cluster centre
    mask(7, 0) = true;

    Finder finder({8, 8}, 10.0);
    finder.set_mask(mask.view());

    SECTION("The cluster grows around the masked centre") {
        finder.find_clusters(img.view());
        auto hits = finder.steal_hits();
        REQUIRE(hits.size() == 1);
        CHECK(hits.hits[0].size == 8);
        CHECK(hits.hits[0].max == 20.0);
        CHECK_THAT(hits.hits[0].energy,
                   Catch::Matchers::WithinAbs(160.0, 1e-12));
        CHECK(pixels_of(hits, 0).count({3, 4}) == 0);
        auto labels = finder.labeled();
        CHECK(labels(3, 4) == -1);
        CHECK(labels(7, 0) == -1);
        CHECK(labels(2, 3) == 1);
        CHECK(labels(0, 0) == 0);
    }
    SECTION("Masked pixels are not zeroed") {
        finder.set_erase_clusters(true);
        finder.find_clusters(img.view());
        CHECK(img(3, 4) == 100.0);
        CHECK(img(7, 0) == 50.0);
        CHECK(img(2, 3) == 0.0);
    }
    SECTION("A masked pixel splits a cluster") {
        NDArray<double, 2> line({8, 8}, 0.0);
        line(2, 2) = line(3, 2) = line(4, 2) = 30.0;
        NDArray<bool, 2> middle({8, 8}, false);
        middle(3, 2) = true;
        Finder split({8, 8}, 10.0);
        split.find_clusters(line.view());
        CHECK(split.total_clusters() == 1);
        split.clear_hits();
        split.set_mask(middle.view());
        split.find_clusters(line.view());
        CHECK(split.total_clusters() == 2);
    }
    SECTION("clear_mask restores the full image") {
        finder.clear_mask();
        finder.find_clusters(img.view());
        auto hits = finder.steal_hits();
        REQUIRE(hits.size() == 2);
        CHECK(hits.hits[0].size == 9);
        CHECK(hits.hits[0].max == 100.0);
        CHECK(finder.labeled()(7, 0) == 2);
    }
    SECTION("A mask of the wrong shape is rejected") {
        NDArray<bool, 2> wrong({8, 9}, false);
        CHECK_THROWS_AS(finder.set_mask(wrong.view()), std::runtime_error);
    }
}

TEST_CASE("mask() returns the current mask", "[varcluster]") {
    Finder finder({8, 8}, 10.0);
    CHECK_FALSE(finder.mask().has_value());
    NDArray<bool, 2> mask({8, 8}, false);
    mask(1, 2) = true;
    finder.set_mask(mask.view());
    REQUIRE(finder.mask().has_value());
    CHECK(*finder.mask() == mask);
    finder.clear_mask();
    CHECK_FALSE(finder.mask().has_value());
}

TEST_CASE("A fixed neighbour threshold grows clusters below the seed "
          "threshold",
          "[varcluster]") {
    NDArray<double, 2> img({8, 8}, 0.0);
    for (int r = 1; r <= 5; ++r)
        for (int c = 1; c <= 5; ++c)
            img(r, c) = 8.0;
    for (int r = 2; r <= 4; ++r)
        for (int c = 2; c <= 4; ++c)
            img(r, c) = 20.0;
    img(3, 3) = 100.0;

    Finder finder({8, 8}, 50.0);
    CHECK(finder.threshold() == 50.0);
    CHECK(finder.neighbour_threshold() == 50.0);

    auto size_of_first_hit = [&]() {
        finder.find_clusters(img.view());
        auto hits = finder.steal_hits();
        REQUIRE(hits.size() == 1);
        return hits.hits[0].size;
    };
    CHECK(size_of_first_hit() == 1);

    finder.set_neighbour_threshold(10.0);
    CHECK(finder.neighbour_threshold() == 10.0);
    CHECK(size_of_first_hit() == 9);

    finder.set_neighbour_threshold(5.0);
    CHECK(size_of_first_hit() == 25);

    finder.clear_neighbour_threshold();
    CHECK(finder.neighbour_threshold() == 50.0);
    CHECK(size_of_first_hit() == 1);

    // The neighbour threshold follows the seed threshold while unset
    finder.set_threshold(15.0);
    CHECK(finder.neighbour_threshold() == 15.0);
    CHECK(size_of_first_hit() == 9);
}
