// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/RAII_device.hpp"
#include <cstddef>
#include <cstdint>
#include <cuda_runtime.h>
#include <type_traits>

namespace aare::cuda::detail {

// A fill policy decides what the fill kernels bin for each raw value and what
// per-pixel state they carry through a batch. The kernels call, per pixel:
//
//   Thread t{};
//   if (!Policy::load(view, pix, t)) return;   // false: skip the pixel
//   for each frame of the batch, in order:
//       v = Policy::value(raw, t);             // what is binned
//       count v if it lies in [xmin, xmax)
//       Policy::after_value(raw, v, params, t) // every value, in range or not
//   Policy::store(view, pix, t);
//
// Window placement in fill_window also calls value(), read-only.
//
// A policy provides:
//   input_type, axis_type    frame type and floating-point axis type
//   Buffers                  owning per-pixel device state (move-only)
//   View                     trivially copyable pointers into Buffers, passed
//                            to the kernels by value
//   Params                   trivially copyable per-launch scalars
//   Thread                   the per-pixel state one thread keeps in
//                            registers between load() and store()
//   static Buffers allocate(size_t npix)
//   static View make_view(const Buffers &)
//   __device__ static bool load(const View &, size_t pix, Thread &)
//   __device__ static axis_type value(input_type raw, const Thread &)
//   __device__ static void after_value(input_type raw, axis_type v,
//                                      const Params &, Thread &)
//   __device__ static void store(const View &, size_t pix, const Thread &)

/// Axis type of PlainFill for frames of InputType: the frame type if floating
/// point, float otherwise.
template <typename InputType>
using plain_axis_t =
    std::conditional_t<std::is_floating_point_v<InputType>, InputType, float>;

/// Bins the raw value converted to the axis type, with no per-pixel state:
/// the plain per-pixel histogram of PixelHistogramCUDA.
template <typename InputType, typename AxisType> struct PlainFill {
    // The conversion to the axis type is exact for integers of up to 16 bits.
    static_assert(std::is_floating_point_v<InputType> ||
                      (std::is_integral_v<InputType> && sizeof(InputType) <= 2),
                  "PlainFill: InputType must be floating point or an integer "
                  "of at most 16 bits");

    using input_type = InputType;
    using axis_type = AxisType;
    struct Buffers {};
    struct View {};
    struct Params {};
    struct Thread {};

    static Buffers allocate(size_t) { return {}; }
    static View make_view(const Buffers &) { return {}; }

    __device__ static bool load(const View &, size_t, Thread &) { return true; }
    __device__ static axis_type value(input_type raw, const Thread &) {
        return static_cast<axis_type>(raw);
    }
    __device__ static void after_value(input_type, axis_type, const Params &,
                                       Thread &) {}
    __device__ static void store(const View &, size_t, const Thread &) {}
};

/// Pedestal tracking, as PedestalTrackingPixelHistogram on the CPU: bins the
/// residual float(raw) - mean, then, if |residual| < n_sigma * sigma, feeds
/// raw into the pixel's pedestal, FastPedestal's exponential moving average:
///   sum  += raw   - sum  / n
///   sum2 += raw^2 - sum2 / n
///   mean  = float(sum / n)
/// with 1/n stored, as FastPedestal does. Every operation is rounded as on
/// the CPU (no fused multiply-add), so histograms and pedestals equal the CPU
/// class's bit for bit. sigma only changes in tracking_sigma.
struct TrackingFill {
    using input_type = uint16_t;
    using axis_type = float;

    struct Buffers {
        DeviceBuffer<double> sum;   ///< ~ n * E[X]
        DeviceBuffer<double> sum2;  ///< ~ n * E[X^2]
        DeviceBuffer<float> mean;   ///< float(sum / n), subtracted from raw
        DeviceBuffer<float> sigma;  ///< gate width, set by tracking_sigma
        DeviceBuffer<uint8_t> mask; ///< empty: no mask; 1: skip the pixel
    };
    struct View {
        double *sum;
        double *sum2;
        float *mean;
        const float *sigma;
        const uint8_t *mask; ///< nullptr: no mask
    };
    struct Params {
        double inv_samples; ///< 1.0 / n_samples
        float n_sigma;      ///< <= 0: no tracking
    };
    struct Thread {
        double sum;
        double sum2;
        float mean;
        float sigma;
    };

    static Buffers allocate(size_t npix) {
        return {DeviceBuffer<double>(npix), DeviceBuffer<double>(npix),
                DeviceBuffer<float>(npix), DeviceBuffer<float>(npix),
                DeviceBuffer<uint8_t>{}};
    }
    static View make_view(const Buffers &b) {
        return {b.sum.get(), b.sum2.get(), b.mean.get(), b.sigma.get(),
                b.mask.get()};
    }

    __device__ static bool load(const View &v, size_t pix, Thread &t) {
        if (v.mask && v.mask[pix])
            return false; // masked: no histogram, no tracking
        t.sum = v.sum[pix];
        t.sum2 = v.sum2[pix];
        t.mean = v.mean[pix];
        t.sigma = v.sigma[pix];
        return true;
    }
    __device__ static axis_type value(input_type raw, const Thread &t) {
        return static_cast<float>(raw) - t.mean;
    }
    __device__ static void after_value(input_type raw, axis_type residual,
                                       const Params &q, Thread &t) {
        if (q.n_sigma > 0.0f && t.sigma > 0.0f &&
            fabsf(residual) < q.n_sigma * t.sigma) {
            const double x = static_cast<double>(raw);
            t.sum =
                __dadd_rn(t.sum, __dsub_rn(x, __dmul_rn(t.sum, q.inv_samples)));
            t.sum2 =
                __dadd_rn(t.sum2, __dsub_rn(__dmul_rn(x, x),
                                            __dmul_rn(t.sum2, q.inv_samples)));
            t.mean = __double2float_rn(__dmul_rn(t.sum, q.inv_samples));
        }
    }
    __device__ static void store(const View &v, size_t pix, const Thread &t) {
        v.sum[pix] = t.sum;
        v.sum2[pix] = t.sum2;
        v.mean[pix] = t.mean;
    }
};

/// sigma = sqrt(max(E[X^2] - E[X]^2, 0)) of each pixel's pedestal, rounded
/// exactly as FastPedestal::std() rounds it (one fused multiply-add, as its
/// std::fma). One thread per pixel. A template so that the header can be
/// included in several translation units (separable compilation).
template <typename SumType>
__global__ void tracking_sigma(const SumType *__restrict__ sum,
                               const SumType *__restrict__ sum2,
                               float *__restrict__ sigma, size_t npix,
                               SumType inv_samples) {
    static_assert(std::is_same_v<SumType, double>,
                  "tracking_sigma: the CPU pedestal sums are double");
    const size_t pix =
        static_cast<size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (pix >= npix)
        return;
    const double m = __dmul_rn(sum[pix], inv_samples);
    const double var = __fma_rn(-m, m, __dmul_rn(sum2[pix], inv_samples));
    sigma[pix] = __double2float_rn(__dsqrt_rn(var < 0.0 ? 0.0 : var));
}

} // namespace aare::cuda::detail
