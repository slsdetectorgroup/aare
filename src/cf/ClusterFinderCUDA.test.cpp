// SPDX-License-Identifier: MPL-2.0
#include "ClusterFinderCUDA_testimpl.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>

using namespace aare;

namespace {

template <typename C> void sort_by_position(std::vector<C> &v) {
    std::sort(v.begin(), v.end(), [](const C &a, const C &b) {
        return std::make_pair(a.y, a.x) < std::make_pair(b.y, b.x);
    });
}

template <typename C> bool identical(std::vector<C> a, std::vector<C> b) {
    if (a.size() != b.size())
        return false;
    sort_by_position(a);
    sort_by_position(b);
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].x != b[i].x || a[i].y != b[i].y || a[i].data != b[i].data)
            return false;
    return true;
}

template <typename R> void check(const R &r) {
    REQUIRE(r.manual.size() == r.driver.size());
    size_t total = 0;
    for (size_t f = 0; f < r.manual.size(); ++f) {
        INFO("frame " << f << ": manual " << r.manual[f].size() << " driver "
                      << r.driver[f].size());
        REQUIRE(identical(r.manual[f], r.driver[f]));
        total += r.manual[f].size();
    }
    // Guard against a vacuous pass: the synthetic frames carry photons.
    REQUIRE(total > 0);
}

} // namespace

// FixedWindow::launch is the public contract for code that owns its device
// memory. Driving it by hand on raw cudaMalloc buffers must give exactly what
// ClusterFinderCUDA gives over the same frames. Odd frame dimensions keep the
// partial-block and halo paths in play.
TEST_CASE("FixedWindow::launch on caller-owned buffers matches the driver",
          "[cuda]") {
    SECTION("3x3") { check(test_cuda::run_fixed_window_3x3(61, 97, 1000, 20)); }
    SECTION("9x9") { check(test_cuda::run_fixed_window_9x9(61, 97, 1000, 20)); }
}
