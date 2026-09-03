// SPDX-License-Identifier: MPL-2.0

#include "aare/ClusterFinderMT.hpp"
#include "aare/Cluster.hpp"
#include "aare/ClusterCollector.hpp"
#include "aare/File.hpp"

#include "test_config.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <memory>

using namespace aare;

// wrapper function to access private member variables for testing
template <typename ClusterType, typename FRAME_TYPE = uint16_t,
          typename PEDESTAL_TYPE = double>
class ClusterFinderMTWrapper
    : public ClusterFinderMT<ClusterType, FRAME_TYPE, PEDESTAL_TYPE> {

  public:
    ClusterFinderMTWrapper(Shape<2> image_size, PEDESTAL_TYPE nSigma = 5.0,
                           size_t capacity = 2000, size_t n_threads = 3,
                           size_t queue_depth = 16)
        : ClusterFinderMT<ClusterType, FRAME_TYPE, PEDESTAL_TYPE>(
              image_size, nSigma, capacity, n_threads, queue_depth) {}

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
    size_t n_frames_pd = 1;
    size_t n_frames = 10;

    using ClusterType = Cluster<int32_t, 3, 3>;

    ClusterFinderMTWrapper<ClusterType> cf(
        {static_cast<int64_t>(file.rows()), static_cast<int64_t>(file.cols())},
        5, 2000, n_frames_pd,
        n_threads); // no idea what frame type is!!! default uint16_t

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
    // CHECK(clustervec.size() == ) //dont know how many clusters to expect
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
