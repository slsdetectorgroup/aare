// SPDX-License-Identifier: MPL-2.0

#include "aare/ClusterFinder.hpp"
#include "aare/Cluster.hpp"
#include "aare/ClusterCollector.hpp"
#include "aare/ClusterFile.hpp"
#include "aare/ClusterFileSink.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

using namespace aare;

namespace {

using ClusterType = Cluster<int32_t, 3, 3>;

constexpr size_t n_threads = 2;
constexpr size_t queue_depth = 16;
constexpr size_t min_pedestal_samples = 4;
const Shape<2> image_size{10, 10};

// Pedestal of 100 with a noise of 2, so a 130 ADU pixel is one cluster at
// nSigma 5.
template <typename CF> void push_pedestal(CF &cf) {
    NDArray<uint16_t, 2> low(image_size, 98);
    NDArray<uint16_t, 2> high(image_size, 102);
    for (size_t i = 0; i < min_pedestal_samples / 2; ++i) {
        cf.push_pedestal_frame(low.view());
        cf.push_pedestal_frame(high.view());
    }
}

template <typename CF> void push_hits(CF &cf, uint64_t n_frames) {
    NDArray<uint16_t, 2> hit(image_size, 100);
    hit(5, 5) = 130;
    for (uint64_t i = 0; i < n_frames; ++i) {
        cf.find_clusters(hit.view(), i + 1);
    }
}

class TemporaryPath {
  public:
    TemporaryPath() {
        const auto unique =
            std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path() /
                 ("aare-cluster-finder-" + std::to_string(unique) + ".clust");
    }
    TemporaryPath(const TemporaryPath &) = delete;
    TemporaryPath &operator=(const TemporaryPath &) = delete;
    ~TemporaryPath() {
        std::error_code error;
        std::filesystem::remove(m_path, error);
    }
    const std::filesystem::path &path() const { return m_path; }

  private:
    std::filesystem::path m_path;
};

} // namespace

static_assert(is_cluster_sink_v<ClusterCollector<ClusterType>>);
static_assert(is_cluster_sink_v<ClusterFileSink<ClusterType>>);
static_assert(!is_cluster_sink_v<ClusterVector<ClusterType>>);
static_assert(!is_cluster_sink_v<int>);
static_assert(
    std::is_same_v<ClusterFinder<ClusterCollector<ClusterType>>::ClusterType,
                   ClusterType>);

TEST_CASE("ClusterFinder with a ClusterCollector collects all frames") {
    const uint64_t n_frames = 20;
    ClusterFinder<ClusterCollector<ClusterType>> cf(
        image_size, 5.0, 200, n_threads, queue_depth, min_pedestal_samples);
    REQUIRE(cf.running());

    push_pedestal(cf);
    REQUIRE(cf.pedestal_ready());
    push_hits(cf, n_frames);

    CHECK_THROWS_AS(cf.pedestal(), std::runtime_error);
    cf.stop();
    CHECK_FALSE(cf.running());

    auto clusters = cf.sink()->steal_clusters();
    REQUIRE(clusters.size() == n_frames);

    std::vector<int32_t> frame_numbers;
    for (const auto &frame_clusters : clusters) {
        CHECK(frame_clusters.size() == 1);
        frame_numbers.push_back(frame_clusters.frame_number());
    }
    std::sort(frame_numbers.begin(), frame_numbers.end());
    for (size_t i = 0; i < frame_numbers.size(); ++i) {
        CHECK(frame_numbers[i] == static_cast<int32_t>(i + 1));
    }

    // Stop is idempotent and the pedestal is reachable once stopped.
    cf.stop();
    CHECK(cf.pedestal().shape() == image_size);
}

TEST_CASE("ClusterFinder destructor stops a running finder and sink") {
    ClusterFinder<ClusterCollector<ClusterType>> cf(
        image_size, 5.0, 200, n_threads, queue_depth, min_pedestal_samples);
    push_pedestal(cf);
    push_hits(cf, 5);
    // no explicit stop, the destructor must join all threads
}

TEST_CASE("ClusterFinder with a ClusterFileSink writes all frames") {
    const uint64_t n_frames = 12;
    TemporaryPath tmp;
    {
        ClusterFinder<ClusterFileSink<ClusterType>> cf(
            image_size, 5.0, 200, n_threads, queue_depth, min_pedestal_samples,
            tmp.path());
        push_pedestal(cf);
        push_hits(cf, n_frames);
        cf.stop();
    }

    ClusterFile<ClusterType> file(tmp.path());
    size_t n_read = 0;
    while (auto frame = file.read_frame()) {
        CHECK(frame->size() == 1);
        CHECK(frame->frame_number() >= 1);
        CHECK(frame->frame_number() <= static_cast<int32_t>(n_frames));
        ++n_read;
    }
    CHECK(n_read == n_frames);
}
