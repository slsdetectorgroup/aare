#pragma once
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"

//Lets see if we need to hide it behind a pimpl
#include <boost/histogram.hpp>
#include <cstdint>
namespace bh = boost::histogram;

namespace aare {
class PixelHistogram{
    using Axes = std::tuple<
    bh::axis::regular<double, bh::use_default, bh::use_default, bh::axis::option::none_t>,
    bh::axis::integer<int, bh::use_default, bh::axis::option::none_t>,
    bh::axis::integer<int, bh::use_default, bh::axis::option::none_t>
    >;
    using Hist = bh::histogram<Axes, bh::dense_storage<int32_t>>;
    Hist hist_;
    
    public:
    PixelHistogram(int rows, int cols, int n_bins, double xmin, double xmax);
    void fill(const NDView<double,2>& image);
    NDArray<int32_t,3> hdata() const;
    NDArray<double,1> bin_centers() const;
    NDArray<double,1> bin_edges() const;
};

} // namespace aare
