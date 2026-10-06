#include "aare/hist/PixelHistogram.hpp"
#include "aare/hist/PixelHistogramOpenMP.hpp"
#include <benchmark/benchmark.h>
#include <random>

using namespace aare;

class TestHistogram : public benchmark::Fixture {
  public:
    std::vector<NDArray<float, 2>> images;
    void SetUp(::benchmark::State &state) {

        std::cout << "In Fixture Setup" << std::endl;

        std::mt19937 gen(rd()); // to seed mersenne twister.
        std::uniform_real_distribution<float> dist(xmin, xmax);

        images.resize(num_images);

        //

        for (size_t i = 0; i < num_images; ++i) {
            NDArray<float, 2> image({rows, cols});
            for (ssize_t r = 0; r < rows; ++r) {
                for (ssize_t c = 0; c < cols; ++c) {
                    image(r, c) = dist(gen);
                }
            }
            images[i] = std::move(image);
        }
    }

    // void TearDown(::benchmark::State& state) {
    // }

    const ssize_t rows = 512;
    const ssize_t cols = 1024;
    const size_t n_bins = 200;
    const float xmin = 600.0f;
    const float xmax = 1400.0f;
    const size_t num_images = 1000;

  private:
    std::random_device rd{};
};

BENCHMARK_DEFINE_F(TestHistogram, process_histogram_openmp)
(benchmark::State &st) {
    const int num_threads = st.range(0);

    PixelHistogramOpenMP hist(rows, cols, n_bins, xmin, xmax, num_threads);
    for (auto _ : st) {
        // This code gets timed
        for (const auto &img : images) {
            hist.fill_async(NDArray<float, 2>(img));
        }
        auto result = hist.values();
        benchmark::DoNotOptimize(result);
    }
}

BENCHMARK_DEFINE_F(TestHistogram, process_histogram)(benchmark::State &st) {
    const int num_threads = st.range(0);
    PixelHistogram hist(rows, cols, n_bins, xmin, xmax, num_threads);
    for (auto _ : st) {
        // This code gets timed
        for (const auto &img : images) {
            hist.fill_async(NDArray<float, 2>(img));
        }
        auto result = hist.values();
        benchmark::DoNotOptimize(result);
    }
}

BENCHMARK_REGISTER_F(TestHistogram, process_histogram)
    ->DenseRange(1, 20)
    ->Iterations(10); // 1 to 20 threads

BENCHMARK_REGISTER_F(TestHistogram, process_histogram_openmp)
    ->DenseRange(1, 20)
    ->Iterations(10); // 1 to 20 threads

// BENCHMARK_MAIN();