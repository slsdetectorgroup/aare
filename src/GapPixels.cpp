// SPDX-License-Identifier: MPL-2.0
#include "aare/GapPixels.hpp"

#include <fmt/format.h>

namespace aare {
namespace detail {

GapLayout gap_layout(DetectorType detector, bool quad, const GapPixels &gaps) {
    if (detector != DetectorType::Jungfrau && detector != DetectorType::Eiger) {
        throw std::invalid_argument(
            LOCATION +
            "Gap pixels are only supported for Jungfrau and Eiger detectors");
    }
    if (gaps.chip_gap < 0) {
        throw std::invalid_argument(LOCATION + "Chip gap must not be negative");
    }
    const ModuleGaps module_gaps =
        gaps.module_gaps.value_or(ModuleGaps{gaps.chip_gap, gaps.chip_gap});
    if (module_gaps.x < 0 || module_gaps.y < 0) {
        throw std::invalid_argument(LOCATION +
                                    "Module gaps must not be negative");
    }
    if (gaps.split_counts && gaps.chip_gap != 2) {
        throw std::invalid_argument(
            LOCATION + "Splitting counts requires a chip gap of 2");
    }

    constexpr ssize_t chip = 256;
    const ssize_t module_x =
        (detector == DetectorType::Eiger && quad) ? 512 : 1024;
    constexpr ssize_t module_y = 512;
    return {{chip, module_x, gaps.chip_gap, module_gaps.x},
            {chip, module_y, gaps.chip_gap, module_gaps.y}};
}

std::array<ssize_t, 2> gapped_shape(const ROI &roi, const GapLayout &layout) {
    return {layout.y.extent(roi.ymin, roi.ymax),
            layout.x.extent(roi.xmin, roi.xmax)};
}

void validate_roi(const ROI &roi) {
    if (roi.xmin < 0 || roi.ymin < 0 || roi.xmax < roi.xmin ||
        roi.ymax < roi.ymin) {
        throw std::invalid_argument(
            LOCATION + fmt::format("Invalid ROI x [{}, {}) y [{}, {})",
                                   roi.xmin, roi.xmax, roi.ymin, roi.ymax));
    }
}

void validate_destination(const std::array<ssize_t, 2> &shape, const ROI &roi,
                          const GapLayout &layout) {
    validate_roi(roi);
    const auto expected = gapped_shape(roi, layout);
    if (shape != expected) {
        throw std::invalid_argument(
            LOCATION +
            fmt::format("Destination shape ({}, {}) does not match the gapped "
                        "ROI shape ({}, {})",
                        shape[0], shape[1], expected[0], expected[1]));
    }
}

std::mt19937_64 make_generator(std::optional<uint64_t> seed) {
    if (seed) {
        return std::mt19937_64(*seed);
    }
    std::random_device device;
    return std::mt19937_64((static_cast<uint64_t>(device()) << 32) ^ device());
}

} // namespace detail

std::array<ssize_t, 2> gapped_shape(const ROI &roi, DetectorType detector,
                                    bool quad, const GapPixels &gaps) {
    const auto layout = detail::gap_layout(detector, quad, gaps);
    detail::validate_roi(roi);
    return detail::gapped_shape(roi, layout);
}

} // namespace aare
