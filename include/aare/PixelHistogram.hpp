#pragma once
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"

//Lets see if we need to hide it behind a pimpl
#include <boost/histogram.hpp>
#include <cstdint>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
namespace bh = boost::histogram;

namespace aare {
class PixelHistogram {
  public:
    using StorageType = uint16_t;
    using AxisType = float;

  private:
    using Axes = std::tuple<
        bh::axis::regular<AxisType, bh::use_default, bh::use_default,
                          bh::axis::option::none_t>,
        bh::axis::integer<int, bh::use_default, bh::axis::option::none_t>,
        bh::axis::integer<int, bh::use_default, bh::axis::option::none_t>>;
    using Hist = bh::histogram<Axes, bh::dense_storage<StorageType>>;
    int rows_;
    int cols_;
    int n_threads_;
    const AxisType xmin_;
    const AxisType xmax_;
    std::vector<Hist> partial_hists_;
    
    // Thread pool members
    std::vector<std::thread> workers_;
    std::mutex work_mutex_;
    std::condition_variable work_cv_;
    std::condition_variable done_cv_;
    const NDView<AxisType, 2>* current_image_;
    std::atomic<int> completed_threads_;
    std::atomic<bool> stop_workers_;
    int work_generation_;
    
    // Private worker thread method
    void worker_loop(int thread_id);
    int row_start(int thread_id) const;
    int row_count(int thread_id) const;

  public:
    PixelHistogram(int rows, int cols, int n_bins, double xmin, double xmax, int n_threads = 1);
    ~PixelHistogram();
    void fill(const NDView<AxisType, 2> &image);
    NDArray<StorageType, 3> hdata() const;
    NDArray<AxisType, 1> bin_centers() const;
    NDArray<AxisType, 1> bin_edges() const;
};

} // namespace aare
