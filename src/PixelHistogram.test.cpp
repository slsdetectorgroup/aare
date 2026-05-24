#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <chrono>
#include <cstdint>
#include <random>
#include <thread>
#include <vector>

#include "test_config.hpp"
#include "test_macros.hpp"

#include "aare/PixelHistogram.hpp"

using aare::PixelHistogram;
using aare::NDArray;
using aare::NDView;

TEST_CASE("Fill one pixel of a 5x10 histogram"){
    PixelHistogram hist(5, 10, 20, 0.0, 10.0);
    NDArray<float, 2> image({5, 10}, -1.0); //Need to fill with -1 to not generate counts
    
    image(2, 3) = 5.7; // This should go into bin 11 (since bins are [0-0.5), [0.5-1.0), ..., [9.5-10.0))
    
    hist.fill(image.view());
    
    auto hdata = hist.hdata();
    REQUIRE(hdata.shape(0) == 5);
    REQUIRE(hdata.shape(1) == 10);
    REQUIRE(hdata.shape(2) == 20);
    
    // Check that the correct bin for pixel (2,3) has count 1
    CHECK(hdata(2, 3, 11) == 1);
    
    // Check that all other bins are zero
    for (ssize_t row = 0; row < hdata.shape(0); ++row) {
        for (ssize_t col = 0; col < hdata.shape(1); ++col) {
            for (ssize_t bin = 0; bin < hdata.shape(2); ++bin) {
                if (!(row == 2 && col == 3 && bin == 11)) {
                    CHECK(hdata(row, col, bin) == 0);
                }
            }
        }
    }
}

TEST_CASE("Fill pixels with uneven partial histogram row slices"){
    PixelHistogram hist(5, 4, 10, 0.0, 10.0, 3);
    NDArray<float, 2> image({5, 4}, -1.0);

    image(0, 0) = 0.2;
    image(1, 1) = 1.2;
    image(2, 2) = 2.2;
    image(3, 3) = 3.2;
    image(4, 0) = 4.2;

    hist.fill(image.view());

    auto hdata = hist.hdata();
    REQUIRE(hdata.shape(0) == 5);
    REQUIRE(hdata.shape(1) == 4);
    REQUIRE(hdata.shape(2) == 10);

    CHECK(hdata(0, 0, 0) == 1);
    CHECK(hdata(1, 1, 1) == 1);
    CHECK(hdata(2, 2, 2) == 1);
    CHECK(hdata(3, 3, 3) == 1);
    CHECK(hdata(4, 0, 4) == 1);

    for (ssize_t row = 0; row < hdata.shape(0); ++row) {
        for (ssize_t col = 0; col < hdata.shape(1); ++col) {
            for (ssize_t bin = 0; bin < hdata.shape(2); ++bin) {
                const bool expected = (row == 0 && col == 0 && bin == 0) ||
                                      (row == 1 && col == 1 && bin == 1) ||
                                      (row == 2 && col == 2 && bin == 2) ||
                                      (row == 3 && col == 3 && bin == 3) ||
                                      (row == 4 && col == 0 && bin == 4);
                if (!expected) {
                    CHECK(hdata(row, col, bin) == 0);
                }
            }
        }
    }
}

TEST_CASE("Row partitioning handles rows < n_threads * ceil(rows/n_threads)") {
    // Regression test for the pre-existing row partitioning bug: with the
    // old ceil(rows / n_threads) scheme, rows=17 and n_threads=8 left
    // trailing threads with negative row counts and threw
    // "stop >= start required" from boost::histogram during construction.
    // The current scheme distributes the remainder so every thread gets
    // at least floor(rows / n_threads) rows. We also exercise the case
    // where n_threads > rows, which is clamped down to rows.
    const auto p = GENERATE(table<int, int>({
        {17, 8},   // previously broken: 8 threads * ceil(17/8) = 24 > 17
        {17, 5},   // previously broken: 5 threads * ceil(17/5) = 20 > 17
        {13, 4},   // previously broken: 4 * ceil(13/4) = 16 > 13
        {17, 32},  // n_threads > rows -> clamped to rows
        {1, 8},    // n_threads > rows -> single thread, single row
        {6, 6},    // balanced
    }));
    const int rows = std::get<0>(p);
    const int n_threads = std::get<1>(p);
    const int cols = 3;
    const int n_bins = 4;
    constexpr float xmin = 0.0f;
    constexpr float xmax = 4.0f;

    PixelHistogram hist(rows, cols, n_bins, xmin, xmax, n_threads);

    // Put one in-range value per row so we can verify every row is covered.
    NDArray<float, 2> img({rows, cols}, -1.0f); // -1 is out of range
    for (ssize_t r = 0; r < rows; ++r) {
        img(r, 0) = static_cast<float>(r % n_bins) + 0.5f;
    }
    hist.fill(img.view());

    auto h = hist.hdata();
    REQUIRE(h.shape(0) == rows);
    REQUIRE(h.shape(1) == cols);
    REQUIRE(h.shape(2) == n_bins);

    for (ssize_t r = 0; r < rows; ++r) {
        const int expected_bin = static_cast<int>(r % n_bins);
        for (ssize_t c = 0; c < cols; ++c) {
            for (ssize_t b = 0; b < n_bins; ++b) {
                const uint16_t want =
                    (c == 0 && b == expected_bin) ? uint16_t{1} : uint16_t{0};
                if (h(r, c, b) != want) {
                    INFO("rows=" << rows << " n_threads=" << n_threads
                         << " r=" << r << " c=" << c << " b=" << b
                         << " got=" << h(r, c, b) << " want=" << want);
                    CHECK(h(r, c, b) == want);
                }
            }
        }
    }
}

TEST_CASE("Random fills match a reference implementation") {
    // End-to-end correctness check: compare the implementation against a
    // simple per-pixel reference for several thread counts. Values are
    // sampled slightly wider than [xmin, xmax) so the out-of-range filter
    // is also exercised.
    constexpr int rows = 17;
    constexpr int cols = 23;
    constexpr int n_bins = 32;
    constexpr float xmin = -1.5f;
    constexpr float xmax = 4.5f;
    const int n_threads = GENERATE(1, 2, 4, 8);

    PixelHistogram hist(rows, cols, n_bins, xmin, xmax, n_threads);

    std::mt19937 rng(0xC0FFEE);
    std::uniform_real_distribution<float> dist(xmin - 0.5f, xmax + 0.5f);

    constexpr int n_frames = 4;
    std::vector<NDArray<float, 2>> frames;
    frames.reserve(n_frames);
    for (int f = 0; f < n_frames; ++f) {
        NDArray<float, 2> img({rows, cols}, 0.0f);
        for (ssize_t r = 0; r < rows; ++r) {
            for (ssize_t c = 0; c < cols; ++c) {
                img(r, c) = dist(rng);
            }
        }
        frames.push_back(std::move(img));
    }

    NDArray<uint32_t, 3> expected({rows, cols, n_bins}, 0u);
    const float inv_range = static_cast<float>(n_bins) / (xmax - xmin);
    for (const auto& img : frames) {
        for (ssize_t r = 0; r < rows; ++r) {
            for (ssize_t c = 0; c < cols; ++c) {
                const float v = img(r, c);
                if (!(v >= xmin) || !(v < xmax)) continue;
                int bin = static_cast<int>((v - xmin) * inv_range);
                if (bin >= n_bins) bin = n_bins - 1;
                if (bin < 0) bin = 0;
                expected(r, c, bin) += 1;
            }
        }
    }

    for (const auto& img : frames) {
        hist.fill(img.view());
    }
    auto h = hist.hdata();

    REQUIRE(h.shape(0) == rows);
    REQUIRE(h.shape(1) == cols);
    REQUIRE(h.shape(2) == n_bins);

    bool all_match = true;
    for (ssize_t r = 0; r < rows && all_match; ++r) {
        for (ssize_t c = 0; c < cols && all_match; ++c) {
            for (ssize_t b = 0; b < n_bins && all_match; ++b) {
                if (h(r, c, b) != expected(r, c, b)) {
                    all_match = false;
                    INFO("n_threads=" << n_threads << " r=" << r
                         << " c=" << c << " b=" << b
                         << " got=" << h(r, c, b)
                         << " expected=" << expected(r, c, b));
                    CHECK(h(r, c, b) == expected(r, c, b));
                }
            }
        }
    }
    CHECK(all_match);
}

TEST_CASE("Async fill matches sync fill") {
    // Submit a stream of random frames through fill_async and compare
    // against the same frames processed by fill() on a separate histogram.
    constexpr int rows = 19;
    constexpr int cols = 23;
    constexpr int n_bins = 16;
    constexpr float xmin = -1.0f;
    constexpr float xmax = 3.0f;
    const int n_threads = GENERATE(1, 2, 4);
    constexpr int n_frames = 32;
    // Pick a small queue capacity so the producer trips the backpressure
    // path at least a few times during the run.
    constexpr std::size_t max_pending = 4;

    PixelHistogram async_hist(rows, cols, n_bins, xmin, xmax, n_threads, max_pending);
    PixelHistogram sync_hist(rows, cols, n_bins, xmin, xmax, n_threads);

    std::mt19937 rng(0xA5A5A5A5);
    std::uniform_real_distribution<float> dist(xmin - 0.25f, xmax + 0.25f);

    for (int f = 0; f < n_frames; ++f) {
        NDArray<float, 2> img({rows, cols}, 0.0f);
        for (ssize_t r = 0; r < rows; ++r)
            for (ssize_t c = 0; c < cols; ++c)
                img(r, c) = dist(rng);
        sync_hist.fill(img.view());
        async_hist.fill_async(std::move(img));
    }
    // hdata() calls flush() internally, but exercise the explicit path too.
    async_hist.flush();
    CHECK(async_hist.pending() == 0);

    auto a = async_hist.hdata();
    auto s = sync_hist.hdata();
    REQUIRE(a.shape(0) == s.shape(0));
    REQUIRE(a.shape(1) == s.shape(1));
    REQUIRE(a.shape(2) == s.shape(2));

    bool all_match = true;
    for (ssize_t r = 0; r < rows && all_match; ++r) {
        for (ssize_t c = 0; c < cols && all_match; ++c) {
            for (ssize_t b = 0; b < n_bins && all_match; ++b) {
                if (a(r, c, b) != s(r, c, b)) {
                    all_match = false;
                    INFO("r=" << r << " c=" << c << " b=" << b
                         << " async=" << a(r, c, b) << " sync=" << s(r, c, b));
                    CHECK(a(r, c, b) == s(r, c, b));
                }
            }
        }
    }
    CHECK(all_match);
}

TEST_CASE("fill_async with mismatched shape throws") {
    PixelHistogram hist(8, 8, 16, 0.0, 1.0, 2);
    NDArray<float, 2> bad({4, 4}, 0.0f);
    CHECK_THROWS_AS(hist.fill_async(std::move(bad)), std::invalid_argument);
}

TEST_CASE("Destructor drains pending async fills") {
    // Submit more frames than the queue can hold so backpressure kicks in,
    // then immediately let the histogram go out of scope and verify that
    // the merged hdata() matches the reference computed sequentially.
    constexpr int rows = 11;
    constexpr int cols = 7;
    constexpr int n_bins = 8;
    constexpr float xmin = 0.0f;
    constexpr float xmax = 1.0f;
    constexpr int n_frames = 12;
    constexpr std::size_t max_pending = 2;

    std::vector<NDArray<float, 2>> frames;
    frames.reserve(n_frames);
    std::mt19937 rng(0xDEADBEEF);
    std::uniform_real_distribution<float> dist(xmin, xmax);
    for (int f = 0; f < n_frames; ++f) {
        NDArray<float, 2> img({rows, cols}, 0.0f);
        for (ssize_t r = 0; r < rows; ++r)
            for (ssize_t c = 0; c < cols; ++c)
                img(r, c) = dist(rng);
        frames.push_back(std::move(img));
    }

    NDArray<aare::PixelHistogram::StorageType, 3> snapshot({rows, cols, n_bins}, uint16_t{0});
    {
        PixelHistogram hist(rows, cols, n_bins, xmin, xmax, 2, max_pending);
        for (auto& img : frames) {
            // Move a copy so we can also build the reference below.
            NDArray<float, 2> copy({rows, cols}, 0.0f);
            std::memcpy(copy.data(), img.data(), copy.total_bytes());
            hist.fill_async(std::move(copy));
        }
        // No explicit flush(); destructor must drain.
        // Capture hdata() *after* the loop but inside the scope so it
        // observes everything that was submitted (hdata flushes too).
        snapshot = hist.hdata();
    }

    PixelHistogram reference(rows, cols, n_bins, xmin, xmax, 2);
    for (const auto& img : frames) reference.fill(img.view());
    auto expected = reference.hdata();

    bool all_match = true;
    for (ssize_t r = 0; r < rows && all_match; ++r) {
        for (ssize_t c = 0; c < cols && all_match; ++c) {
            for (ssize_t b = 0; b < n_bins && all_match; ++b) {
                if (snapshot(r, c, b) != expected(r, c, b)) {
                    all_match = false;
                    INFO("r=" << r << " c=" << c << " b=" << b
                         << " got=" << snapshot(r, c, b)
                         << " expected=" << expected(r, c, b));
                    CHECK(snapshot(r, c, b) == expected(r, c, b));
                }
            }
        }
    }
    CHECK(all_match);
}
