// SPDX-License-Identifier: MPL-2.0
#include "aare/ClusterFinder.hpp"
#include "aare/Pedestal.hpp"
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <chrono>
#include <random>

using namespace aare;

namespace {

// Deterministic frame: a pedestal near 1000 with a few ADU of noise that
// differs in every pixel and frame, and one photon whose position follows
// the frame index
NDArray<uint16_t, 2> finder_test_frame(Shape<2> shape, uint32_t frame_index) {
    NDArray<uint16_t, 2> frame(shape);
    uint32_t x = frame_index * 2654435761u + 12345u;
    for (ssize_t i = 0; i < frame.size(); ++i) {
        x = x * 1664525u + 1013904223u;
        frame[i] = static_cast<uint16_t>(1000 + (x >> 29));
    }
    const ssize_t row = 2 + frame_index % 5;
    const ssize_t col = 2 + (frame_index * 3) % 6;
    frame(row, col) = static_cast<uint16_t>(frame(row, col) + 300);
    return frame;
}

// Use a frame as a measurement does: for the pedestal until that is ready,
// then to find clusters, which keeps updating the pedestal
template <typename Finder>
void process_test_frame(Finder &finder, Shape<2> shape, uint32_t frame_index) {
    auto frame = finder_test_frame(shape, frame_index);
    if (finder.pedestal_ready()) {
        finder.find_clusters(frame.view(), frame_index);
    } else {
        finder.push_pedestal_frame(frame.view());
    }
}

template <typename ClusterType>
bool same_clusters(const ClusterVector<ClusterType> &a,
                   const ClusterVector<ClusterType> &b) {
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].x != b[i].x || a[i].y != b[i].y || a[i].data != b[i].data)
            return false;
    }
    return true;
}

// The threshold update_threshold() would compute now
template <typename Finder>
NDArray<double, 2> scaled_noise(Finder &finder, double n_sigma) {
    NDArray<double, 2> threshold = finder.noise();
    for (ssize_t i = 0; i < threshold.size(); ++i) {
        threshold[i] *= n_sigma;
    }
    return threshold;
}

bool same_finder_state(const ClusterFinderState &a,
                       const ClusterFinderState &b) {
    return a.cluster_dtype == b.cluster_dtype &&
           a.cluster_size_x == b.cluster_size_x &&
           a.cluster_size_y == b.cluster_size_y && a.n_sigma == b.n_sigma &&
           a.threshold == b.threshold && a.pedestal.dtype == b.pedestal.dtype &&
           a.pedestal.n_samples == b.pedestal.n_samples &&
           a.pedestal.cur_samples == b.pedestal.cur_samples &&
           a.pedestal.sum == b.pedestal.sum &&
           a.pedestal.sum2 == b.pedestal.sum2;
}

} // namespace

// TODO! Find a way to test the cluster finder

// class ClusterFinderUnitTest : public ClusterFinder {
//   public:
//     ClusterFinderUnitTest(int cluster_sizeX, int cluster_sizeY, double nSigma
//     = 5.0, double threshold = 0.0)
//         : ClusterFinder(cluster_sizeX, cluster_sizeY, nSigma, threshold) {}
//     double get_c2() { return c2; }
//     double get_c3() { return c3; }
//     auto get_threshold() { return m_threshold; }
//     auto get_nSigma() { return m_nSigma; }
//     auto get_cluster_sizeX() { return m_cluster_sizeX; }
//     auto get_cluster_sizeY() { return m_cluster_sizeY; }
// };

// TEST_CASE("test ClusterFinder constructor") {
//     ClusterFinderUnitTest cf(55, 100);
//     REQUIRE(cf.get_cluster_sizeX() == 55);
//     REQUIRE(cf.get_cluster_sizeY() == 100);
//     REQUIRE(cf.get_threshold() == 0.0);
//     REQUIRE(cf.get_nSigma() == 5.0);
//     double c2 = sqrt((100 + 1) / 2 * (55 + 1) / 2);
//     double c3 = sqrt(55 * 100);
//     // REQUIRE(compare_floats<double>(cf.get_c2(), c2));
//     // REQUIRE(compare_floats<double>(cf.get_c3(), c3));
//     REQUIRE_THAT(cf.get_c2(), Catch::Matchers::WithinRel(c2, 1e-9));
//     REQUIRE_THAT(cf.get_c3(), Catch::Matchers::WithinRel(c3, 1e-9));
// }

TEST_CASE("Construct a cluster finder") {
    ClusterFinder clusterFinder({400, 400});
    // REQUIRE(clusterFinder.get_cluster_sizeX() == 3);
    // REQUIRE(clusterFinder.get_cluster_sizeY() == 3);
    // REQUIRE(clusterFinder.get_threshold() == 1);
    // REQUIRE(clusterFinder.get_nSigma() == 1);
}

TEST_CASE("nSigma set before the pedestal is ready is used for the threshold") {
    const Shape<2> image_size{10, 10};
    const size_t min_pedestal_samples = 4;

    // Pedestal of 100 with a noise of 2 and a 30 ADU hit: a cluster at the
    // default nSigma of 5, below threshold at nSigma 50.
    NDArray<uint16_t, 2> low(image_size, 98);
    NDArray<uint16_t, 2> high(image_size, 102);
    NDArray<uint16_t, 2> hit(image_size, 100);
    hit(5, 5) = 130;

    const double nSigma = GENERATE(5.0, 50.0);
    const size_t expected_clusters = nSigma == 5.0 ? 1 : 0;

    ClusterFinder<Cluster<int32_t, 3, 3>> cf(image_size, 5.0, 100,
                                             min_pedestal_samples);
    REQUIRE_FALSE(cf.pedestal_ready());
    REQUIRE_NOTHROW(cf.set_nSigma(nSigma));
    REQUIRE(cf.get_nSigma() == nSigma);

    for (size_t i = 0; i < min_pedestal_samples / 2; ++i) {
        cf.push_pedestal_frame(low.view());
        REQUIRE_FALSE(cf.pedestal_ready());
        cf.push_pedestal_frame(high.view());
    }
    REQUIRE(cf.pedestal_ready());

    cf.find_clusters(hit.view());
    REQUIRE(cf.steal_clusters().size() == expected_clusters);
}

TEST_CASE("update_threshold throws until the pedestal is ready") {
    ClusterFinder<Cluster<int32_t, 3, 3>> cf({10, 10});
    REQUIRE_THROWS_AS(cf.update_threshold(), std::runtime_error);
}

TEMPLATE_TEST_CASE("cluster finder restored from a state continues identically",
                   "[cluster-finder]", double, float) {
    using Finder = ClusterFinder<Cluster<int32_t, 3, 3>, uint16_t, TestType>;
    const Shape<2> shape{10, 12};
    const size_t n_samples = 5;
    const TestType n_sigma = 4;

    // Empty, initializing, just ready and after finding clusters
    for (const uint32_t frames_before : {0u, 3u, 5u, 12u}) {
        CAPTURE(frames_before);
        Finder original(shape, n_sigma, 100, n_samples);
        uint32_t frame_index = 0;
        for (; frame_index < frames_before; ++frame_index) {
            process_test_frame(original, shape, frame_index);
        }
        // Not part of the state
        const auto found_before = original.steal_clusters().size();
        REQUIRE((found_before > 0) == (frames_before > n_samples));

        const auto state = original.state();
        REQUIRE(state.cluster_dtype == Dtype::INT32);
        REQUIRE(state.cluster_size_x == 3);
        REQUIRE(state.cluster_size_y == 3);
        REQUIRE(state.n_sigma == 4.0);
        REQUIRE(state.threshold.shape() == shape);
        REQUIRE(state.pedestal.dtype == Dtype(typeid(TestType)));
        REQUIRE(state.pedestal.n_samples == n_samples);
        REQUIRE(state.pedestal.sum.shape() == shape);

        Finder constructed(state);
        // nSigma and min_pedestal_samples are replaced by those of the state
        Finder assigned(shape, 50, 100, 99);
        assigned.set_state(state);

        for (auto *restored : {&constructed, &assigned}) {
            REQUIRE(restored->get_nSigma() == n_sigma);
            REQUIRE(restored->pedestal_ready() == original.pedestal_ready());
            REQUIRE(same_finder_state(restored->state(), state));
        }

        // Enough frames to make every case ready and find clusters after
        for (uint32_t i = 0; i < 10; ++i, ++frame_index) {
            process_test_frame(original, shape, frame_index);
            process_test_frame(constructed, shape, frame_index);
            process_test_frame(assigned, shape, frame_index);
        }

        const auto expected_state = original.state();
        const auto expected_clusters = original.steal_clusters();
        REQUIRE(expected_clusters.size() > 0);
        for (auto *restored : {&constructed, &assigned}) {
            REQUIRE(same_finder_state(restored->state(), expected_state));
            REQUIRE(restored->pedestal() == original.pedestal());
            REQUIRE(restored->noise() == original.noise());
            REQUIRE(
                same_clusters(restored->steal_clusters(), expected_clusters));
        }
    }
}

TEST_CASE("cluster finder state keeps the threshold as it was computed",
          "[cluster-finder]") {
    using Finder = ClusterFinder<Cluster<int32_t, 3, 3>>;
    const Shape<2> shape{10, 12};
    Finder original(shape, 4.0, 100, 5);
    for (uint32_t frame_index = 0; frame_index < 5; ++frame_index) {
        process_test_frame(original, shape, frame_index);
    }
    const auto threshold = original.state().threshold;
    REQUIRE(threshold == scaled_noise(original, 4.0));

    // Finding clusters updates the pedestal and leaves the threshold alone
    for (uint32_t frame_index = 5; frame_index < 20; ++frame_index) {
        process_test_frame(original, shape, frame_index);
    }
    const auto state = original.state();
    REQUIRE(state.threshold == threshold);
    REQUIRE_FALSE(state.threshold == scaled_noise(original, 4.0));

    Finder restored(state);
    REQUIRE(restored.state().threshold == threshold);
    restored.update_threshold();
    REQUIRE(restored.state().threshold == scaled_noise(original, 4.0));
}

TEST_CASE("cluster finder state does not depend on the value types",
          "[cluster-finder]") {
    const Shape<2> shape{10, 12};
    ClusterFinder<Cluster<int32_t, 3, 3>, uint16_t, double> source(shape, 4.0,
                                                                   100, 5);
    for (uint32_t frame_index = 0; frame_index < 12; ++frame_index) {
        process_test_frame(source, shape, frame_index);
    }
    const auto state = source.state();

    // The dtypes of the state only record where it came from
    ClusterFinder<Cluster<double, 3, 3>, uint16_t, float> other(state);
    const auto other_state = other.state();
    REQUIRE(other_state.cluster_dtype == Dtype::DOUBLE);
    REQUIRE(other_state.pedestal.dtype == Dtype::FLOAT);
    REQUIRE(other_state.n_sigma == state.n_sigma);
    REQUIRE(other_state.pedestal.sum == state.pedestal.sum);
    REQUIRE(other_state.pedestal.sum2 == state.pedestal.sum2);
    for (ssize_t i = 0; i < state.threshold.size(); ++i) {
        REQUIRE(other_state.threshold[i] ==
                static_cast<double>(static_cast<float>(state.threshold[i])));
    }

    // As does a state without dtypes
    auto untyped = state;
    untyped.cluster_dtype = Dtype::NONE;
    untyped.pedestal.dtype = Dtype::NONE;
    ClusterFinder<Cluster<int32_t, 3, 3>> from_untyped(untyped);
    REQUIRE(same_finder_state(from_untyped.state(), state));
}

TEST_CASE("cluster finder drops clusters it still holds when given a state",
          "[cluster-finder]") {
    using Finder = ClusterFinder<Cluster<int32_t, 3, 3>>;
    const Shape<2> shape{10, 12};
    Finder finder(shape, 4.0, 100, 5);
    for (uint32_t frame_index = 0; frame_index < 5; ++frame_index) {
        process_test_frame(finder, shape, frame_index);
    }
    const auto state = finder.state();
    for (uint32_t frame_index = 5; frame_index < 9; ++frame_index) {
        process_test_frame(finder, shape, frame_index);
    }

    finder.set_state(state);
    const auto clusters = finder.steal_clusters();
    REQUIRE(clusters.size() == 0);
    REQUIRE(clusters.capacity() >= 100);
}

TEST_CASE("cluster finder rejects an invalid state and stays unchanged",
          "[cluster-finder]") {
    using Finder = ClusterFinder<Cluster<int32_t, 3, 3>>;
    const Shape<2> shape{10, 12};
    Finder finder(shape, 4.0, 100, 5);
    Finder reference(shape, 4.0, 100, 5);
    for (uint32_t frame_index = 0; frame_index < 9; ++frame_index) {
        process_test_frame(finder, shape, frame_index);
        process_test_frame(reference, shape, frame_index);
    }
    const auto before = finder.state();

    // Only a finder with the same cluster size takes the state
    ClusterFinder<Cluster<int32_t, 5, 5>> larger(shape, 4.0, 100, 5);
    const auto larger_state = larger.state();
    REQUIRE(larger_state.cluster_size_x == 5);
    REQUIRE_THROWS_AS(finder.set_state(larger_state), std::runtime_error);
    REQUIRE_THROWS_AS(larger.set_state(before), std::runtime_error);
    REQUIRE_THROWS_AS((ClusterFinder<Cluster<int32_t, 5, 5>>(before)),
                      std::runtime_error);
    REQUIRE(same_finder_state(larger.state(), larger_state));

    auto wrong_threshold = before;
    wrong_threshold.threshold = NDArray<double, 2>({12, 10}, 0.0);
    REQUIRE_THROWS_AS(finder.set_state(wrong_threshold), std::runtime_error);
    REQUIRE_THROWS_AS(Finder(wrong_threshold), std::runtime_error);

    auto wrong_pedestal = before;
    wrong_pedestal.n_sigma = 99;
    wrong_pedestal.pedestal.sum = NDArray<double, 2>({12, 10}, 0.0);
    wrong_pedestal.pedestal.sum2 = NDArray<double, 2>({12, 10}, 0.0);
    REQUIRE_THROWS_AS(finder.set_state(wrong_pedestal), std::runtime_error);

    auto no_samples = before;
    no_samples.n_sigma = 99;
    no_samples.pedestal.n_samples = 0;
    no_samples.pedestal.cur_samples = 0;
    REQUIRE_THROWS_AS(finder.set_state(no_samples), std::runtime_error);
    REQUIRE_THROWS_AS(Finder(no_samples), std::runtime_error);

    // An empty state has no shape to construct from
    REQUIRE_THROWS_AS(Finder(ClusterFinderState{}), std::runtime_error);

    REQUIRE(same_finder_state(finder.state(), before));
    // The clusters found before the failed calls are still there
    const auto kept = finder.steal_clusters();
    REQUIRE(kept.size() > 0);
    REQUIRE(same_clusters(kept, reference.steal_clusters()));
}

// TEST_CASE("test cluster finder") {
//     aare::Pedestal pedestal(10, 10, 5);
//     NDArray<double, 2> frame({10, 10});
//     frame = 0;
//     ClusterFinder clusterFinder(3, 3, 1, 1); // 3x3 cluster, 1 nSigma, 1
//     threshold

//     auto clusters =
//     clusterFinder.find_clusters_without_threshold(frame.span(), pedestal);

//     REQUIRE(clusters.size() == 0);

//     frame(5, 5) = 10;
//     clusters = clusterFinder.find_clusters_without_threshold(frame.span(),
//     pedestal); REQUIRE(clusters.size() == 1); REQUIRE(clusters[0].x == 5);
//     REQUIRE(clusters[0].y == 5);
//     for (int i = 0; i < 3; i++) {
//         for (int j = 0; j < 3; j++) {
//             if (i == 1 && j == 1)
//                 REQUIRE(clusters[0].get<double>(i * 3 + j) == 10);
//             else
//                 REQUIRE(clusters[0].get<double>(i * 3 + j) == 0);
//         }
//     }
// }