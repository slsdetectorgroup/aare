// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <cstddef>
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

} // namespace aare::cuda::detail
