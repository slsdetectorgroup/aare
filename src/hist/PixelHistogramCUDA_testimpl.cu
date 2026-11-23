// SPDX-License-Identifier: MPL-2.0
#include "PixelHistogramCUDA_testimpl.hpp"
#include "aare/hist/PixelHistogramCUDA.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace aare::cuda {

// Writes count = b * npix + p, unique per cell, into the pixel-major
// accumulator, then checks that values() returns it in the same layout and
// capped at StorageType's max. Throws on the first mismatch. The check is a
// lambda because only this friend function may touch the private members.
void known_pattern_cap_impl() {
    auto check = [](auto storage_tag, size_t nrows, size_t ncols, size_t nbins,
                    const char *type_name) {
        using StorageType = decltype(storage_tag);
        PixelHistogramCUDA<StorageType> hist(nrows, ncols, nbins, 0.0f, 1.0f);
        const size_t npix = nrows * ncols;

        std::vector<uint32_t> pattern(npix * nbins);
        // Counts 0 .. npix * nbins - 1, all different, so a misplaced count
        // shows up.
        for (size_t p = 0; p < npix; ++p)
            for (size_t b = 0; b < nbins; ++b)
                pattern[p * nbins + b] = static_cast<uint32_t>(b * npix + p);

        // Copied on the compute stream: after the constructor's memset,
        // before values()'s cap kernel.
        CUDA_CHECK(cudaMemcpyAsync(
            hist.d_histogram.get(), pattern.data(), hist.d_histogram.bytes(),
            cudaMemcpyHostToDevice, hist.compute_stream.get()));

        const auto out = hist.values();
        for (size_t p = 0; p < npix; ++p) {
            for (size_t b = 0; b < nbins; ++b) {
                uint64_t want = b * npix + p;
                if constexpr (std::is_integral_v<StorageType>)
                    want = std::min<uint64_t>(
                        want, std::numeric_limits<StorageType>::max());
                const auto got = out(p / ncols, p % ncols, b);
                if (got != static_cast<StorageType>(want))
                    throw std::runtime_error(
                        std::string(type_name) + ", " + std::to_string(nrows) +
                        "x" + std::to_string(ncols) + " px, " +
                        std::to_string(nbins) + " bins: pixel " +
                        std::to_string(p) + ", bin " + std::to_string(b) +
                        ": expected " + std::to_string(want) + ", got " +
                        std::to_string(static_cast<double>(got)));
            }
        }
    };

    // Odd sizes leave a partial block in the cap kernel. Counts reach ~88k:
    // capped for uint8_t and uint16_t, exact for uint32_t and float.
    check(uint8_t{}, 37, 53, 45, "uint8_t");
    check(uint16_t{}, 37, 53, 45, "uint16_t");
    check(uint32_t{}, 37, 53, 45, "uint32_t");
    check(float{}, 37, 53, 45, "float");
    check(uint32_t{}, 1, 1, 1, "uint32_t");
}

} // namespace aare::cuda

namespace aare::test_cuda {

void check_known_pattern() { cuda::known_pattern_cap_impl(); }

template <typename H, typename In>
DeviceResult<H> run_gpu(NDView<In, 3> frames, size_t nbins, float xmin,
                        float xmax, size_t batch_size, bool window) {
    using D = std::conditional_t<sizeof(H) == 8, uint64_t, uint32_t>;
    cuda::PixelHistogramCUDA<H, D, In> hist(
        static_cast<size_t>(frames.shape(1)),
        static_cast<size_t>(frames.shape(2)), nbins, xmin, xmax, batch_size,
        window);
    hist.register_input_buffer(frames.data(), frames.total_bytes());
    hist.fill_histogram(frames);
    // Unpin while the histogram lives: must wait for the fill's copies.
    hist.unregister_input_buffer();
    return {hist.bin_centers(), hist.bin_edges(), hist.values()};
}

#define INSTANTIATE(H, In)                                                     \
    template DeviceResult<H> run_gpu<H, In>(NDView<In, 3>, size_t, float,      \
                                            float, size_t, bool);
INSTANTIATE(uint8_t, float)
INSTANTIATE(uint16_t, float)
INSTANTIATE(uint32_t, float)
INSTANTIATE(uint64_t, float)
INSTANTIATE(float, float)
INSTANTIATE(double, float)
INSTANTIATE(uint32_t, uint16_t)
#undef INSTANTIATE

std::vector<NDArray<uint32_t, 3>>
run_gpu_in_parts(NDView<float, 3> frames, size_t nbins, float xmin, float xmax,
                 size_t batch_size, const std::vector<size_t> &ends,
                 bool window) {
    cuda::PixelHistogramCUDA<uint32_t> hist(
        static_cast<size_t>(frames.shape(1)),
        static_cast<size_t>(frames.shape(2)), nbins, xmin, xmax, batch_size,
        window);
    hist.register_input_buffer(frames.data(), frames.total_bytes());

    std::vector<NDArray<uint32_t, 3>> snapshots;
    size_t begin = 0;
    for (size_t end : ends) {
        hist.fill_histogram(frames.sub_view(static_cast<ssize_t>(begin),
                                            static_cast<ssize_t>(end)));
        snapshots.push_back(hist.values());
        begin = end;
    }
    return snapshots;
}

bool constructor_throws(size_t nbins, size_t batch_size, bool window) {
    try {
        cuda::PixelHistogramCUDA<uint32_t> hist(2, 3, nbins, 0.0f, 1.0f,
                                                batch_size, window);
    } catch (const std::invalid_argument &) {
        return true;
    }
    return false;
}

FailedRegistration fill_after_failed_registration() {
    const size_t nrows = 3, ncols = 5, nbins = 10, batch = 4;
    NDArray<float, 3> frames(
        {6, static_cast<ssize_t>(nrows), static_cast<ssize_t>(ncols)}, 0.5f);
    using Hist = cuda::PixelHistogramCUDA<uint32_t>;
    Hist a(nrows, ncols, nbins, 0.0f, 1.0f, batch);
    Hist a2(nrows, ncols, nbins, 0.0f, 1.0f, batch);
    Hist b(nrows, ncols, nbins, 0.0f, 1.0f, batch);

    FailedRegistration r{false, false, 0, static_cast<uint64_t>(frames.size())};
    a.register_input_buffer(frames.data(), frames.total_bytes());
    try {
        a2.register_input_buffer(frames.data(), frames.total_bytes());
    } catch (const std::runtime_error &) {
        r.registration_threw = true;
    }
    try {
        b.fill_histogram(frames.view());
    } catch (const std::runtime_error &) {
        r.fill_threw = true;
    }
    const auto values = b.values();
    for (ssize_t i = 0; i < values.size(); ++i)
        r.counted += values[i];
    return r;
}

namespace {

// The exception can't cross into the Catch2 file as a REQUIRE_THROWS, so
// report whether values_pixel(row, col) threw std::out_of_range.
template <typename Hist>
bool throws_out_of_range(const Hist &hist, size_t row, size_t col) {
    try {
        hist.values_pixel(row, col);
    } catch (const std::out_of_range &) {
        return true;
    }
    return false;
}

} // namespace

PixelQueries
run_gpu_values_pixel(NDView<float, 3> frames, size_t nbins, float xmin,
                     float xmax, size_t batch_size,
                     const std::vector<std::pair<size_t, size_t>> &pixels) {
    const auto nrows = static_cast<size_t>(frames.shape(1));
    const auto ncols = static_cast<size_t>(frames.shape(2));
    cuda::PixelHistogramCUDA<uint8_t> hist(nrows, ncols, nbins, xmin, xmax,
                                           batch_size);
    hist.register_input_buffer(frames.data(), frames.total_bytes());
    hist.fill_histogram(frames);

    PixelQueries out{hist.values(), {}, false, false};
    for (const auto &[row, col] : pixels)
        out.pixels.push_back(hist.values_pixel(row, col));
    out.row_out_of_range_throws = throws_out_of_range(hist, nrows, 0);
    out.col_out_of_range_throws = throws_out_of_range(hist, 0, ncols);
    return out;
}

} // namespace aare::test_cuda
