// SPDX-License-Identifier: MPL-2.0
#include "aare/GapPixels.hpp"

#include <fmt/format.h>

#include <limits>

namespace aare {
namespace detail {

namespace {

constexpr ssize_t chip = 256;
constexpr ssize_t module_x = 1024;
constexpr ssize_t module_y = 512;

// Gap sizes and detector coordinates are bounded so that gapped coordinates
// and gapped image sizes cannot overflow ssize_t. A gapped coordinate is at
// most (1 + 2 * max_gap / chip) times the detector coordinate, and the image
// size is the product of two such extents.
constexpr ssize_t max_gap = 1024;
constexpr ssize_t max_coordinate = ssize_t{1} << 28;
constexpr ssize_t max_extent = max_coordinate * (1 + 2 * max_gap / chip);
static_assert(max_extent <= std::numeric_limits<ssize_t>::max() / max_extent,
              "gapped image sizes must fit in ssize_t");

} // namespace

GapLayout gap_layout(const GapPixels &gaps) {
    if (gaps.chip_gap < 0) {
        throw std::invalid_argument(LOCATION + "Chip gap must not be negative");
    }
    const ModuleGaps module_gaps = gaps.module_gaps.value_or(ModuleGaps{});
    if (module_gaps.x < 0 || module_gaps.y < 0) {
        throw std::invalid_argument(LOCATION +
                                    "Module gaps must not be negative");
    }
    if (gaps.chip_gap > max_gap) {
        throw std::invalid_argument(
            LOCATION +
            fmt::format("Chip gap must not exceed {} pixels", max_gap));
    }
    if (module_gaps.x > max_gap || module_gaps.y > max_gap) {
        throw std::invalid_argument(
            LOCATION +
            fmt::format("Module gaps must not exceed {} pixels", max_gap));
    }
    if (gaps.split_counts && gaps.chip_gap != 2) {
        throw std::invalid_argument(
            LOCATION + "Splitting counts requires a chip gap of 2");
    }
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
    if (roi.xmax > max_coordinate || roi.ymax > max_coordinate) {
        throw std::invalid_argument(
            LOCATION +
            fmt::format("ROI coordinates must not exceed {}", max_coordinate));
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

std::array<ssize_t, 2> gapped_shape(const ROI &roi, const GapPixels &gaps) {
    const auto layout = detail::gap_layout(gaps);
    detail::validate_roi(roi);
    return detail::gapped_shape(roi, layout);
}

} // namespace aare
