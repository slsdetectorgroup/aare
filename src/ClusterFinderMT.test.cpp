// SPDX-License-Identifier: MPL-2.0

#include "aare/ClusterFinderMT.hpp"
#include "aare/Cluster.hpp"
#include "aare/ClusterCollector.hpp"
#include "aare/File.hpp"

#include "test_config.hpp"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <memory>
#include <tuple>
#include <vector>

using namespace aare;

// wrapper function to access private member variables for testing
template <typename ClusterType, typename FRAME_TYPE = uint16_t,
          typename PEDESTAL_TYPE = double>
class ClusterFinderMTWrapper
    : public ClusterFinderMT<ClusterType, FRAME_TYPE, PEDESTAL_TYPE> {

  public:
    ClusterFinderMTWrapper(Shape<2> image_size, PEDESTAL_TYPE nSigma = 5.0,
                           size_t capacity = 2000, size_t n_threads = 3,
                           size_t queue_depth = 16,
                           size_t minimum_pedestal_samples = 1000)
        : ClusterFinderMT<ClusterType, FRAME_TYPE, PEDESTAL_TYPE>(
              image_size, nSigma, capacity, n_threads, queue_depth,
              minimum_pedestal_samples) {}

    size_t get_m_input_queues_size() const {
        return this->m_input_queues.size();
    }

    size_t get_m_frame_pools_size() const { return this->m_frame_pools.size(); }

    size_t get_frame_pool_depth(size_t thread_index) const {
        return this->m_frame_pools[thread_index]->size();
    }

    size_t get_m_output_queues_size() const {
        return this->m_output_queues.size();
    }

    size_t get_m_cluster_finders_size() const {
        return this->m_cluster_finders.size();
    }

    bool m_output_queues_are_empty() const {
        for (auto &queue : this->m_output_queues) {
            if (!queue->isEmpty())
                return false;
        }
        return true;
    }

    bool m_input_queues_are_empty() const {
        for (auto &queue : this->m_input_queues) {
            if (!queue->isEmpty())
                return false;
        }
        return true;
    }

    bool m_sink_is_empty() const { return this->m_sink.isEmpty(); }

    size_t m_sink_size() const { return this->m_sink.sizeGuess(); }
};

TEST_CASE("multithreaded cluster finder", "[.with-data]") {
    auto fpath = test_data_path() / "raw/moench03/cu_half_speed_master_4.json";

    REQUIRE(std::filesystem::exists(fpath));

    File file(fpath);

    size_t n_threads = 2;
    size_t minimum_pedestal_samples = 1;
    size_t n_frames = 10;

    using ClusterType = Cluster<int32_t, 3, 3>;

    ClusterFinderMTWrapper<ClusterType> cf(
        {static_cast<int64_t>(file.rows()), static_cast<int64_t>(file.cols())},
        5, 2000, n_threads, 16, minimum_pedestal_samples);

    CHECK(cf.get_m_input_queues_size() == n_threads);
    CHECK(cf.get_m_output_queues_size() == n_threads);
    CHECK(cf.get_m_cluster_finders_size() == n_threads);
    CHECK(cf.m_output_queues_are_empty() == true);
    CHECK(cf.m_input_queues_are_empty() == true);

    auto frame = file.read_frame();
    cf.push_pedestal_frame(frame.view<uint16_t>());

    for (size_t i = 0; i < n_frames; ++i) {
        frame = file.read_frame();
        cf.find_clusters(frame.view<uint16_t>());
    }

    cf.stop();

    CHECK(cf.m_output_queues_are_empty() == true);
    CHECK(cf.m_input_queues_are_empty() == true);

    CHECK(cf.m_sink_size() == n_frames);
    ClusterCollector<ClusterType> clustercollector(&cf);

    clustercollector.stop();

    CHECK(cf.m_sink_size() == 0);

    auto clustervec = clustercollector.steal_clusters();
}

TEST_CASE("frame buffers are recycled when pushing more frames than the pool "
          "holds",
          "[.files]") {
    using ClusterType = Cluster<int32_t, 3, 3>;

    const size_t n_threads = 2;
    const size_t queue_depth = 4;
    const size_t n_frames = 5 * queue_depth;
    const Shape<2> image_size{10, 10};

    ClusterFinderMTWrapper<ClusterType> cf(image_size, 5, 200, n_threads,
                                           queue_depth);

    CHECK(cf.get_m_frame_pools_size() == n_threads);
    for (size_t i = 0; i < n_threads; ++i) {
        CHECK(cf.get_frame_pool_depth(i) == queue_depth);
    }

    NDArray<uint16_t, 2> frame(image_size, 0);

    // More frames than the pool holds, so this only completes if the workers
    // return the buffers to the free list.
    for (size_t i = 0; i < n_frames; ++i) {
        cf.find_clusters(frame.view(), i);
    }

    cf.stop();

    CHECK(cf.m_input_queues_are_empty() == true);
}

TEST_CASE("nSigma set before the pedestal is ready reaches all threads") {
    using ClusterType = Cluster<int32_t, 3, 3>;

    const size_t n_threads = 2;
    const size_t min_pedestal_samples = 4;
    const Shape<2> image_size{10, 10};

    ClusterFinderMTWrapper<ClusterType> cf(image_size, 5, 200, n_threads, 16,
                                           min_pedestal_samples);

    // Pedestal of 100 with a noise of 2 and a 30 ADU hit: a cluster at the
    // default nSigma of 5, below threshold at nSigma 50.
    NDArray<uint16_t, 2> low(image_size, 98);
    NDArray<uint16_t, 2> high(image_size, 102);
    NDArray<uint16_t, 2> hit(image_size, 100);
    hit(5, 5) = 130;

    CHECK_FALSE(cf.pedestal_ready());
    REQUIRE_NOTHROW(cf.set_nSigma(50));

    for (size_t i = 0; i < min_pedestal_samples / 2; ++i) {
        cf.push_pedestal_frame(low.view());
        cf.push_pedestal_frame(high.view());
    }
    CHECK(cf.pedestal_ready());

    for (size_t i = 0; i < n_threads; ++i) {
        cf.find_clusters(hit.view(), i);
    }
    cf.stop();

    ClusterCollector<ClusterType> collector(&cf);
    collector.stop();

    auto clusters = collector.steal_clusters();
    REQUIRE(clusters.size() == n_threads);
    for (const auto &frame_clusters : clusters) {
        CHECK(frame_clusters.size() == 0);
    }
}

TEST_CASE("cluster collector accepts finders with a matching cluster type") {
    using ClusterType = Cluster<int32_t, 3, 3>;
    using Finder = ClusterFinderMTWrapper<ClusterType, uint16_t, float>;
    using OtherFinder =
        ClusterFinderMTWrapper<Cluster<int32_t, 5, 5>, uint16_t, float>;

    static_assert(
        std::is_constructible_v<ClusterCollector<ClusterType>, Finder *>);
    static_assert(
        !std::is_constructible_v<ClusterCollector<ClusterType>, OtherFinder *>);

    Finder cf({10, 10});
    cf.stop();

    ClusterCollector<ClusterType> collector(&cf);
    collector.stop();

    CHECK(collector.steal_clusters().empty());
}

TEST_CASE("cluster collector drains queued clusters when stopped") {
    using ClusterType = Cluster<int32_t, 3, 3>;
    ProducerConsumerQueue<ClusterVector<ClusterType>> source(4);

    for (uint64_t frame_number = 1; frame_number <= 3; ++frame_number) {
        REQUIRE(source.write(ClusterVector<ClusterType>(4, frame_number)));
    }

    ClusterCollector<ClusterType> collector(&source);
    collector.stop();

    auto clusters = collector.steal_clusters();
    REQUIRE(clusters.size() == 3);
    CHECK(source.isEmpty());
    for (size_t i = 0; i < clusters.size(); ++i) {
        CHECK(clusters[i].frame_number() == static_cast<int32_t>(i + 1));
    }
}

namespace {

using StateCluster = Cluster<int32_t, 3, 3>;
using StateFinderMT = ClusterFinderMT<StateCluster>;
constexpr Shape<2> state_shape{10, 12};
constexpr uint32_t state_pedestal_frames = 5;

// Deterministic frame: a pedestal near 1000 with a few ADU of noise that
// differs in every pixel and frame, and one photon whose position follows
// the frame index
NDArray<uint16_t, 2> state_test_frame(uint32_t frame_index) {
    NDArray<uint16_t, 2> frame(state_shape);
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

// The first frames make the pedestal of every thread, the others are data
template <typename Finder>
void process_frames(Finder &finder, uint32_t first, uint32_t last) {
    for (uint32_t frame_index = first; frame_index < last; ++frame_index) {
        auto frame = state_test_frame(frame_index);
        if (frame_index < state_pedestal_frames) {
            finder.push_pedestal_frame(frame.view());
        } else {
            finder.find_clusters(frame.view(), frame_index);
        }
    }
}

// The clusters that reached the sink of a stopped finder, in an order that
// does not depend on which thread was first
using FoundCluster =
    std::tuple<int32_t, uint16_t, uint16_t, std::array<int32_t, 9>>;
std::vector<FoundCluster> sorted_clusters(StateFinderMT &finder) {
    ClusterCollector<StateCluster> collector(&finder);
    collector.stop();
    std::vector<FoundCluster> found;
    for (const auto &frame_clusters : collector.steal_clusters()) {
        for (size_t i = 0; i < frame_clusters.size(); ++i) {
            const auto &cluster = frame_clusters[i];
            found.emplace_back(frame_clusters.frame_number(), cluster.x,
                               cluster.y, cluster.data);
        }
    }
    std::sort(found.begin(), found.end());
    return found;
}

bool same_state(const ClusterFinderState &a, const ClusterFinderState &b) {
    return a.cluster_dtype == b.cluster_dtype &&
           a.cluster_size_x == b.cluster_size_x &&
           a.cluster_size_y == b.cluster_size_y && a.n_sigma == b.n_sigma &&
           a.threshold == b.threshold && a.pedestal.dtype == b.pedestal.dtype &&
           a.pedestal.n_samples == b.pedestal.n_samples &&
           a.pedestal.cur_samples == b.pedestal.cur_samples &&
           a.pedestal.sum == b.pedestal.sum &&
           a.pedestal.sum2 == b.pedestal.sum2;
}

bool same_state(const ClusterFinderMTState &a, const ClusterFinderMTState &b) {
    if (a.next_thread != b.next_thread || a.finders.size() != b.finders.size())
        return false;
    for (size_t i = 0; i < a.finders.size(); ++i) {
        if (!same_state(a.finders[i], b.finders[i]))
            return false;
    }
    return true;
}

// A ready pedestal taken on its own, from other frames than the finders'
FastPedestal<float> dark_pedestal(uint32_t first_frame, uint32_t n_samples) {
    FastPedestal<float> pedestal(static_cast<uint32_t>(state_shape[0]),
                                 static_cast<uint32_t>(state_shape[1]),
                                 n_samples);
    for (uint32_t i = 0; i < n_samples; ++i) {
        auto frame = state_test_frame(first_frame + i);
        pedestal.add_init_frame(frame.view());
    }
    return pedestal;
}

} // namespace

TEST_CASE("multithreaded cluster finder continues identically from a state",
          "[cluster-finder-mt]") {
    const uint32_t n_frames = 41;
    const uint32_t n_before = 21; // 16 data frames, so thread 1 is next

    StateFinderMT uninterrupted(state_shape, 4.0, 200, 3, 16,
                                state_pedestal_frames);
    process_frames(uninterrupted, 0, n_frames);
    uninterrupted.stop();
    const auto expected_state = uninterrupted.state();
    const auto expected_clusters = sorted_clusters(uninterrupted);
    REQUIRE(expected_clusters.size() >= n_frames - state_pedestal_frames);

    StateFinderMT original(state_shape, 4.0, 200, 3, 16, state_pedestal_frames);
    process_frames(original, 0, n_before);
    original.stop();
    const auto state = original.state();
    auto clusters = sorted_clusters(original);

    REQUIRE(state.finders.size() == 3);
    REQUIRE(state.next_thread == 1);
    // Each thread updated its pedestal from the frames it was given
    REQUIRE_FALSE(state.finders[0].pedestal.sum ==
                  state.finders[1].pedestal.sum);
    REQUIRE_FALSE(state.finders[1].pedestal.sum ==
                  state.finders[2].pedestal.sum);

    SECTION("in a finder constructed from the state") {
        StateFinderMT restored(state);
        REQUIRE(restored.n_threads() == 3);
        process_frames(restored, n_before, n_frames);
        restored.stop();

        REQUIRE(same_state(restored.state(), expected_state));
        const auto rest = sorted_clusters(restored);
        clusters.insert(clusters.end(), rest.begin(), rest.end());
        std::sort(clusters.begin(), clusters.end());
        REQUIRE(clusters == expected_clusters);
    }

    SECTION("in an existing finder given the state") {
        // One that holds other data and settings
        StateFinderMT other(state_shape, 7.0, 200, 3, 16, 2);
        for (uint32_t frame_index = 100; frame_index < 110; ++frame_index) {
            auto frame = state_test_frame(frame_index);
            if (frame_index < 102) {
                other.push_pedestal_frame(frame.view());
            } else {
                other.find_clusters(frame.view(), frame_index);
            }
        }
        other.stop();
        sorted_clusters(other);

        other.set_state(state);
        REQUIRE(same_state(other.state(), state));
        other.start();
        process_frames(other, n_before, n_frames);
        other.stop();

        REQUIRE(same_state(other.state(), expected_state));
        const auto rest = sorted_clusters(other);
        clusters.insert(clusters.end(), rest.begin(), rest.end());
        std::sort(clusters.begin(), clusters.end());
        REQUIRE(clusters == expected_clusters);
    }
}

TEST_CASE("multithreaded cluster finder only hands out and takes state while "
          "stopped",
          "[cluster-finder-mt]") {
    StateFinderMT finder(state_shape, 4.0, 200, 2, 16, state_pedestal_frames);
    const auto pedestal = dark_pedestal(100, 8);
    ClusterFinder<StateCluster> single(state_shape, 4.0, 200,
                                       state_pedestal_frames);

    REQUIRE_THROWS_AS(finder.state(), std::runtime_error);
    REQUIRE_THROWS_AS(finder.set_state(ClusterFinderMTState{}),
                      std::runtime_error);
    REQUIRE_THROWS_AS(finder.set_state(single.state()), std::runtime_error);
    REQUIRE_THROWS_AS(finder.set_pedestal(pedestal), std::runtime_error);
    REQUIRE_THROWS_AS(finder.get_pedestal(), std::runtime_error);

    // Stopping and starting again when that is already so does nothing
    finder.stop();
    REQUIRE_NOTHROW(finder.stop());
    REQUIRE_NOTHROW(finder.state());
    finder.start();
    REQUIRE_NOTHROW(finder.start());
    REQUIRE_THROWS_AS(finder.state(), std::runtime_error);
    finder.stop();
}

TEST_CASE("multithreaded cluster finder rejects an invalid state and stays "
          "unchanged",
          "[cluster-finder-mt]") {
    StateFinderMT finder(state_shape, 4.0, 200, 3, 16, state_pedestal_frames);
    process_frames(finder, 0, 12);
    finder.stop();
    const auto state = finder.state();

    SECTION("for another number of threads") {
        auto other = state;
        other.finders.pop_back();
        other.next_thread = 0;
        REQUIRE_THROWS_AS(finder.set_state(other), std::runtime_error);
        REQUIRE_THROWS_AS(finder.set_state(ClusterFinderMTState{}),
                          std::runtime_error);
    }
    SECTION("with a next thread that does not exist") {
        auto other = state;
        other.next_thread = 3;
        REQUIRE_THROWS_AS(finder.set_state(other), std::runtime_error);
        REQUIRE_THROWS_AS(StateFinderMT{other}, std::runtime_error);
    }
    SECTION("with one cluster finder that does not fit") {
        // The last one, so that the others would be set before it
        auto other = state;
        other.finders[0].n_sigma = 9.0;
        other.finders[2].threshold = NDArray<double, 2>({10, 13});
        REQUIRE_THROWS_AS(finder.set_state(other), std::runtime_error);
        REQUIRE_THROWS_AS(StateFinderMT{other}, std::runtime_error);
    }
    SECTION("without cluster finders") {
        REQUIRE_THROWS_AS(StateFinderMT{ClusterFinderMTState{}},
                          std::runtime_error);
    }

    REQUIRE(same_state(finder.state(), state));
}

TEST_CASE("multithreaded cluster finder takes the state of one cluster finder "
          "for one thread or for all",
          "[cluster-finder-mt]") {
    ClusterFinder<StateCluster> single(state_shape, 6.0, 200, 4);
    for (uint32_t frame_index = 200; frame_index < 204; ++frame_index) {
        auto frame = state_test_frame(frame_index);
        single.push_pedestal_frame(frame.view());
    }
    const auto single_state = single.state();

    StateFinderMT finder(state_shape, 4.0, 200, 3, 16, state_pedestal_frames);
    process_frames(finder, 0, 12);
    finder.stop();
    const auto before = finder.state();
    REQUIRE(before.next_thread == 1);

    SECTION("for all threads") {
        finder.set_state(single_state);
        const auto state = finder.state();
        REQUIRE(state.next_thread == 0);
        for (const auto &finder_state : state.finders) {
            REQUIRE(same_state(finder_state, single_state));
        }
    }
    SECTION("for one thread") {
        finder.set_state(single_state, 1);
        const auto state = finder.state();
        REQUIRE(state.next_thread == 1);
        REQUIRE(same_state(state.finders[0], before.finders[0]));
        REQUIRE(same_state(state.finders[1], single_state));
        REQUIRE(same_state(state.finders[2], before.finders[2]));
    }
    SECTION("not for a thread that does not exist") {
        REQUIRE_THROWS_AS(finder.set_state(single_state, 3),
                          std::runtime_error);
        REQUIRE(same_state(finder.state(), before));
    }
}

TEST_CASE("multithreaded cluster finder takes and returns the pedestal of "
          "one thread or of all",
          "[cluster-finder-mt]") {
    const auto pedestal = dark_pedestal(100, 8);
    const auto other_pedestal = dark_pedestal(300, 6);

    // What a cluster finder with nSigma 4 makes of each pedestal
    ClusterFinder<StateCluster> reference(state_shape, 4.0, 200,
                                          state_pedestal_frames);
    reference.set_pedestal(pedestal);
    const auto expected = reference.state();
    reference.set_pedestal(other_pedestal);
    const auto other_expected = reference.state();

    StateFinderMT finder(state_shape, 4.0, 200, 3, 16, state_pedestal_frames);
    process_frames(finder, 0, 12);
    finder.stop();
    const auto before = finder.state();

    finder.set_pedestal(pedestal);
    auto state = finder.state();
    REQUIRE(state.next_thread == before.next_thread);
    for (const auto &finder_state : state.finders) {
        REQUIRE(same_state(finder_state, expected));
    }

    finder.set_pedestal(other_pedestal, 2);
    state = finder.state();
    REQUIRE(same_state(state.finders[0], expected));
    REQUIRE(same_state(state.finders[1], expected));
    REQUIRE(same_state(state.finders[2], other_expected));

    REQUIRE(finder.get_pedestal().state().sum == pedestal.state().sum);
    REQUIRE(finder.get_pedestal(1).state().sum == pedestal.state().sum);
    REQUIRE(finder.get_pedestal(2).state().sum == other_pedestal.state().sum);
    REQUIRE(finder.get_pedestal(2).state().n_samples == 6);

    REQUIRE_THROWS_AS(finder.set_pedestal(pedestal, 3), std::runtime_error);
    REQUIRE_THROWS_AS(finder.get_pedestal(3), std::runtime_error);
    FastPedestal<double> other_shape(10, 13, 8);
    REQUIRE_THROWS_AS(finder.set_pedestal(other_shape), std::runtime_error);
    REQUIRE(same_state(finder.state(), state));
}

TEST_CASE("multithreaded cluster finder processes every frame when its "
          "queues are full",
          "[cluster-finder-mt]") {
    // One buffer per thread, so that every frame waits for the one before
    const size_t n_data_frames = 300;
    StateFinderMT finder(state_shape, 4.0, 200, 2, 1, state_pedestal_frames);
    process_frames(finder, 0, state_pedestal_frames + n_data_frames);
    finder.stop();

    ClusterCollector<StateCluster> collector(&finder);
    collector.stop();
    // One cluster vector reaches the sink for each data frame
    REQUIRE(collector.steal_clusters().size() == n_data_frames);
}
