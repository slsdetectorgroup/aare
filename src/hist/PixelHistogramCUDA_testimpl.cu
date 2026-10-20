// SPDX-License-Identifier: MPL-2.0
#include "PixelHistogramCUDA_testimpl.hpp"
#include "aare/hist/PixelHistogramCUDA.hpp"
#include <type_traits>

namespace aare::test_cuda {

template <typename H>
DeviceResult<H> run_gpu(NDView<float, 3> frames, size_t nbins, float xmin,
                        float xmax, size_t batch_size) {
    using D = std::conditional_t<sizeof(H) == 8, uint64_t, uint32_t>;
    cuda::PixelHistogramCUDA<H, D> hist(static_cast<size_t>(frames.shape(1)),
                                        static_cast<size_t>(frames.shape(2)),
                                        nbins, xmin, xmax, batch_size);
    hist.register_input_buffer(frames.data(), frames.total_bytes());
    hist.fill_histogram(frames);
    hist.unregister_input_buffer();
    return {hist.bin_centers(), hist.bin_edges(), hist.values()};
}

#define INSTANTIATE(H)                                                         \
    template DeviceResult<H> run_gpu<H>(NDView<float, 3>, size_t, float,       \
                                        float, size_t);
INSTANTIATE(uint8_t)
INSTANTIATE(uint16_t)
INSTANTIATE(uint32_t)
INSTANTIATE(uint64_t)
INSTANTIATE(float)
INSTANTIATE(double)
#undef INSTANTIATE

} // namespace aare::test_cuda
