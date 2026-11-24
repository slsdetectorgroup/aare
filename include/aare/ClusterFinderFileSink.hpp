#include "aare/ClusterFinder.hpp"

namespace aare {

/**
 * @brief A ClusterFinder that collects clusters in a ClusterVector in memory
 */
template <typename ClusterType,
          typename = std::enable_if_t<is_cluster_v<ClusterType>>>
class ClusterFinderFileSink
    : public aare::ClusterFinder<aare::ClusterFileSink<ClusterType>> {
  public:
    using Base = aare::ClusterFinder<aare::ClusterFileSink<ClusterType>>;

    explicit ClusterFinderFileSink(std::filesystem::path fname,
                                   aare::Shape<2> image_size,
                                   double nSigma = 5.0, size_t capacity = 2000,
                                   size_t n_threads = 3,
                                   size_t queue_depth = 16,
                                   size_t min_pedestal_samples = 1000)
        : Base(image_size, nSigma, capacity, n_threads, queue_depth,
               min_pedestal_samples, std::move(fname)) {}

    ~ClusterFinderFileSink() = default;
    ClusterFinderFileSink(const ClusterFinderFileSink &) = delete;
    ClusterFinderFileSink &operator=(const ClusterFinderFileSink &) = delete;
    ClusterFinderFileSink(ClusterFinderFileSink &&) = delete;
    ClusterFinderFileSink &operator=(ClusterFinderFileSink &&) = delete;
};

} // namespace aare