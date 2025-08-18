
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
                           size_t capacity = 2000, size_t n_threads = 3)
        : ClusterFinderMT<ClusterType, FRAME_TYPE, PEDESTAL_TYPE>(
              image_size, nSigma, capacity, n_threads) {}

    size_t get_m_input_queues_size() const {
        return this->m_input_queues.size();
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
    auto fpath =
        test_data_path() / "raw/moench03/cu_half_speed_master_4.json";

    REQUIRE(std::filesystem::exists(fpath));

    File file(fpath);

    size_t n_threads = 2;
    size_t n_frames_pd = 10;

    using ClusterType = Cluster<int32_t, 3, 3>;

    ClusterFinderMTWrapper<ClusterType> cf(
        {static_cast<int64_t>(file.rows()), static_cast<int64_t>(file.cols())},
        5, 2000, n_threads); // no idea what frame type is!!! default uint16_t

    CHECK(cf.get_m_input_queues_size() == n_threads);
    CHECK(cf.get_m_output_queues_size() == n_threads);
    CHECK(cf.get_m_cluster_finders_size() == n_threads);
    CHECK(cf.m_output_queues_are_empty() == true);
    CHECK(cf.m_input_queues_are_empty() == true);

    for (size_t i = 0; i < n_frames_pd; ++i) {
        auto frame = file.read_frame();
        cf.find_clusters(frame.view<uint16_t>());
    }

    cf.stop();

    CHECK(cf.m_output_queues_are_empty() == true);
    CHECK(cf.m_input_queues_are_empty() == true);

    CHECK(cf.m_sink_size() == n_frames_pd);
    ClusterCollector<ClusterType> clustercollector(&cf);

    clustercollector.stop();

    CHECK(cf.m_sink_size() == 0);

    auto clustervec = clustercollector.steal_clusters();
    // CHECK(clustervec.size() == ) //dont know how many clusters to expect
}
