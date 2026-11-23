// SPDX-License-Identifier: MPL-2.0

#include "aare/cf/ClusterFinderMT.hpp"
#include "aare/Cluster.hpp"
#include "aare/ClusterCollector.hpp"
#include "aare/File.hpp"

#include "test_config.hpp"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <memory>
#include <thread>
#include <type_traits>
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

TEST_CASE("clusters can be stolen from a running cluster collector") {
    using ClusterType = Cluster<int32_t, 3, 3>;
    ProducerConsumerQueue<ClusterVector<ClusterType>> source(8);
    ClusterCollector<ClusterType> collector(&source);

    const auto write_frames = [&source](uint64_t first, uint64_t last) {
        for (uint64_t frame_number = first; frame_number <= last;
             ++frame_number) {
            REQUIRE(source.write(ClusterVector<ClusterType>(4, frame_number)));
        }
        while (!source.isEmpty()) {
            std::this_thread::yield();
        }
    };

    CHECK(collector.steal_clusters().empty());

    write_frames(1, 3);
    auto clusters = collector.steal_clusters();
    REQUIRE(clusters.size() == 3);
    for (size_t i = 0; i < clusters.size(); ++i) {
        CHECK(clusters[i].frame_number() == static_cast<int32_t>(i + 1));
    }

    // Stolen clusters are gone, later ones are still collected
    write_frames(4, 5);
    clusters = collector.steal_clusters();
    REQUIRE(clusters.size() == 2);
    CHECK(clusters[0].frame_number() == 4);
    CHECK(clusters[1].frame_number() == 5);

    collector.stop();
    CHECK(collector.steal_clusters().empty());
}

TEST_CASE("sync gives a running collector the clusters of all frames") {
    using ClusterType = Cluster<int32_t, 3, 3>;

    const size_t n_threads = 3;
    const size_t min_pedestal_samples = 4;
    const size_t frames_per_round = 50;
    const Shape<2> image_size{10, 10};

    ClusterFinderMT<ClusterType> cf(image_size, 5, 200, n_threads, 16,
                                    min_pedestal_samples);
    ClusterCollector<ClusterType> collector(&cf);

    NDArray<uint16_t, 2> low(image_size, 98);
    NDArray<uint16_t, 2> high(image_size, 102);
    NDArray<uint16_t, 2> hit(image_size, 100);
    hit(5, 5) = 130;

    for (size_t i = 0; i < min_pedestal_samples / 2; ++i) {
        cf.push_pedestal_frame(low.view());
        cf.push_pedestal_frame(high.view());
    }

    uint64_t frame_number = 0;
    for (int round = 0; round < 3; ++round) {
        for (size_t i = 0; i < frames_per_round; ++i) {
            cf.find_clusters(hit.view(), frame_number++);
        }
        cf.sync();

        auto clusters = collector.steal_clusters();
        REQUIRE(clusters.size() == frames_per_round);
        std::vector<int32_t> frame_numbers;
        for (const auto &frame_clusters : clusters) {
            CHECK(frame_clusters.size() == 1);
            frame_numbers.push_back(frame_clusters.frame_number());
        }
        std::sort(frame_numbers.begin(), frame_numbers.end());
        const auto first = static_cast<int32_t>(frame_number) -
                           static_cast<int32_t>(frames_per_round);
        for (size_t i = 0; i < frame_numbers.size(); ++i) {
            CHECK(frame_numbers[i] == first + static_cast<int32_t>(i));
        }
    }

    cf.stop();
    collector.stop();
    CHECK(collector.steal_clusters().empty());
}

TEST_CASE("cluster collector can be started again after a stop") {
    using ClusterType = Cluster<int32_t, 3, 3>;
    ProducerConsumerQueue<ClusterVector<ClusterType>> source(8);
    ClusterCollector<ClusterType> collector(&source);
    collector.start(); // running: does nothing

    REQUIRE(source.write(ClusterVector<ClusterType>(4, 1)));
    collector.stop();
    collector.stop(); // stopped: does nothing
    CHECK(source.isEmpty());

    // Nothing is taken from the queue while stopped
    REQUIRE(source.write(ClusterVector<ClusterType>(4, 2)));
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    CHECK_FALSE(source.isEmpty());

    collector.start();
    REQUIRE(source.write(ClusterVector<ClusterType>(4, 3)));
    collector.stop();

    // Clusters from before the stop are kept
    auto clusters = collector.steal_clusters();
    REQUIRE(clusters.size() == 3);
    for (size_t i = 0; i < clusters.size(); ++i) {
        CHECK(clusters[i].frame_number() == static_cast<int32_t>(i + 1));
    }
}

TEST_CASE("cluster collector stops its thread when destroyed") {
    using ClusterType = Cluster<int32_t, 3, 3>;
    ProducerConsumerQueue<ClusterVector<ClusterType>> source(8);
    {
        ClusterCollector<ClusterType> collector(&source);
        REQUIRE(source.write(ClusterVector<ClusterType>(4, 1)));
    }
    // The destructor waits for the queue to be empty, as stop() does
    CHECK(source.isEmpty());

    static_assert(!std::is_copy_constructible_v<ClusterCollector<ClusterType>>);
    static_assert(!std::is_move_constructible_v<ClusterCollector<ClusterType>>);
}

TEST_CASE("running cluster finder can be destroyed") {
    using ClusterType = Cluster<int32_t, 3, 3>;
    const Shape<2> image_size{10, 10};
    NDArray<uint16_t, 2> frame(image_size, 100);

    {
        ClusterFinderMT<ClusterType> cf(image_size);
    }
    {
        ClusterFinderMT<ClusterType> cf(image_size, 5, 16, 3, 16, 2);
        cf.push_pedestal_frame(frame.view());
        cf.push_pedestal_frame(frame.view());
        cf.find_clusters(frame.view());
    }
    {
        // Stopped: the destructor has nothing to do
        ClusterFinderMT<ClusterType> cf(image_size);
        cf.stop();
        cf.stop();
    }
    {
        ClusterFinderMT<ClusterType> cf(image_size);
        cf.start(); // running: does nothing
        cf.stop();
        cf.start();
    }

    static_assert(!std::is_copy_constructible_v<ClusterFinderMT<ClusterType>>);
    static_assert(!std::is_move_constructible_v<ClusterFinderMT<ClusterType>>);
}

TEST_CASE("cluster finder with a full sink can be destroyed") {
    using ClusterType = Cluster<int32_t, 3, 3>;
    const size_t n_threads = 3;
    const Shape<2> image_size{10, 10};
    NDArray<uint16_t, 2> frame(image_size, 100);

    ClusterFinderMT<ClusterType> cf(image_size, 5, 16, n_threads, 16, 2);
    cf.push_pedestal_frame(frame.view());
    cf.push_pedestal_frame(frame.view());

    // Nothing empties the sink, so it fills up, then the queues of the
    // threads, and the threads wait for room. With 1000 cluster vectors in
    // the sink and 200 in the queue of each thread, these frames leave
    // every thread waiting without blocking find_clusters(). stop() would
    // wait forever, the destructor must not.
    const size_t n_frames = 1000 + n_threads * 200 + n_threads * 4;
    for (size_t i = 0; i < n_frames; ++i) {
        cf.find_clusters(frame.view(), i);
    }
    while (!cf.sink()->isFull()) {
        std::this_thread::yield();
    }
}
