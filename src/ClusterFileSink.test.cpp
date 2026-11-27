// SPDX-License-Identifier: MPL-2.0
#include "aare/ClusterFileSink.hpp"
#include "aare/Cluster.hpp"
#include "aare/ClusterFile.hpp"
#include "aare/ClusterFinderMT.hpp"
#include "temporary_path.hpp"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

using namespace aare;

namespace {

using SinkCluster = Cluster<int32_t, 3, 3>;
using SinkQueue = ProducerConsumerQueue<ClusterVector<SinkCluster>>;

class TemporarySinkFile {
    std::filesystem::path m_path;

  public:
    TemporarySinkFile() {
        m_path = unique_temporary_path("aare-cluster-file-sink", ".clust");
    }
    TemporarySinkFile(const TemporarySinkFile &) = delete;
    TemporarySinkFile &operator=(const TemporarySinkFile &) = delete;
    ~TemporarySinkFile() {
        std::error_code error;
        std::filesystem::remove(m_path, error);
    }
    const std::filesystem::path &path() const { return m_path; }
};

// Frame with n_clusters clusters whose values are given by the frame number
ClusterVector<SinkCluster> make_frame(int32_t frame_number, size_t n_clusters) {
    ClusterVector<SinkCluster> clusters(n_clusters, frame_number);
    for (size_t i = 0; i < n_clusters; ++i) {
        SinkCluster cluster{};
        cluster.x = static_cast<uint16_t>(frame_number);
        cluster.y = static_cast<uint16_t>(i);
        std::fill(std::begin(cluster.data), std::end(cluster.data),
                  frame_number * 10 + static_cast<int32_t>(i));
        clusters.push_back(cluster);
    }
    return clusters;
}

// Frame i of the file must be make_frame(first + i, i % 3)
void check_file(const std::filesystem::path &path, int32_t first,
                int32_t n_frames) {
    ClusterFile<SinkCluster> file(path);
    for (int32_t i = 0; i < n_frames; ++i) {
        const auto read = file.read_frame();
        REQUIRE(read.has_value());
        const auto expected = make_frame(first + i, static_cast<size_t>(i % 3));
        CHECK(read->frame_number() == expected.frame_number());
        REQUIRE(read->size() == expected.size());
        for (size_t j = 0; j < expected.size(); ++j) {
            CHECK((*read)[j].x == expected[j].x);
            CHECK((*read)[j].y == expected[j].y);
            CHECK(std::equal(std::begin((*read)[j].data),
                             std::end((*read)[j].data),
                             std::begin(expected[j].data)));
        }
    }
    CHECK_FALSE(file.read_frame().has_value());
}

} // namespace

TEST_CASE("cluster file sink writes the clusters queued before it started") {
    TemporarySinkFile file;
    SinkQueue source(8);
    for (int32_t i = 0; i < 5; ++i) {
        REQUIRE(source.write(make_frame(1 + i, static_cast<size_t>(i % 3))));
    }

    ClusterFileSink<SinkCluster> sink(&source, file.path());
    sink.stop();

    CHECK(source.isEmpty());
    check_file(file.path(), 1, 5);
}

TEST_CASE("cluster file sink writes the clusters queued while it runs") {
    TemporarySinkFile file;
    SinkQueue source(8);
    ClusterFileSink<SinkCluster> sink(&source, file.path());

    for (int32_t i = 0; i < 40; ++i) {
        auto frame = make_frame(1 + i, static_cast<size_t>(i % 3));
        while (!source.write(std::move(frame))) {
            std::this_thread::yield();
        }
    }
    sink.stop();
    sink.stop(); // stopped: does nothing

    check_file(file.path(), 1, 40);
}

TEST_CASE("cluster file sink stops and closes the file when destroyed") {
    TemporarySinkFile file;
    SinkQueue source(8);
    {
        ClusterFileSink<SinkCluster> sink(&source, file.path());
        for (int32_t i = 0; i < 5; ++i) {
            REQUIRE(
                source.write(make_frame(1 + i, static_cast<size_t>(i % 3))));
        }
    }
    CHECK(source.isEmpty());
    check_file(file.path(), 1, 5);

    static_assert(!std::is_copy_constructible_v<ClusterFileSink<SinkCluster>>);
    static_assert(!std::is_move_constructible_v<ClusterFileSink<SinkCluster>>);
}

TEST_CASE("cluster file sink throws if the file cannot be opened") {
    TemporarySinkFile file;
    SinkQueue source(8);
    REQUIRE(source.write(make_frame(1, 1)));

    // The file name is used as a directory that does not exist
    CHECK_THROWS_AS(
        ClusterFileSink<SinkCluster>(&source, file.path() / "clusters.clust"),
        std::runtime_error);
    // Nothing was taken from the queue
    CHECK_FALSE(source.isEmpty());
}

TEST_CASE("cluster file sink writes the clusters of a cluster finder") {
    TemporarySinkFile file;
    const size_t min_pedestal_samples = 4;
    const Shape<2> image_size{10, 10};
    const int32_t n_frames = 30;

    ClusterFinderMT<SinkCluster> finder(image_size, 5, 200, 3, 16,
                                        min_pedestal_samples);
    ClusterFileSink<SinkCluster> sink(&finder, file.path());

    NDArray<uint16_t, 2> low(image_size, 98);
    NDArray<uint16_t, 2> high(image_size, 102);
    NDArray<uint16_t, 2> hit(image_size, 100);
    hit(5, 5) = 130;
    for (size_t i = 0; i < min_pedestal_samples / 2; ++i) {
        finder.push_pedestal_frame(low.view());
        finder.push_pedestal_frame(high.view());
    }
    for (int32_t i = 0; i < n_frames; ++i) {
        finder.find_clusters(hit.view(), static_cast<uint64_t>(i));
    }
    finder.stop();
    sink.stop();

    ClusterFile<SinkCluster> reader(file.path());
    std::vector<int32_t> frame_numbers;
    while (auto frame = reader.read_frame()) {
        REQUIRE(frame->size() == 1);
        CHECK((*frame)[0].x == 5);
        CHECK((*frame)[0].y == 5);
        frame_numbers.push_back(frame->frame_number());
    }
    std::sort(frame_numbers.begin(), frame_numbers.end());
    REQUIRE(frame_numbers.size() == static_cast<size_t>(n_frames));
    for (int32_t i = 0; i < n_frames; ++i) {
        CHECK(frame_numbers[static_cast<size_t>(i)] == i);
    }
}
