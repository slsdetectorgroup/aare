// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include "aare/ROI.hpp"
#include "aare/defs.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace aare {

/**
 * @brief Pixels inserted at each boundary between modules, replacing the
 * chip gap there.
 */
struct ModuleGaps {
    ssize_t x{};
    ssize_t y{};

    bool operator==(const ModuleGaps &other) const {
        return x == other.x && y == other.y;
    }
    bool operator!=(const ModuleGaps &other) const { return !(*this == other); }
};

/**
 * @brief Configuration for inserting gap pixels into Jungfrau and Eiger
 * images. Chip and module sizes are fixed by the detector type: 256x256
 * pixel chips in 1024x512 pixel modules (512x512 for an Eiger quad).
 *
 * Gap positions are determined in detector coordinates before any ROI, so
 * a ROI only receives the gaps that fall strictly inside it.
 */
struct GapPixels {
    /// @brief Pixels inserted at each chip boundary inside a module. Source
    /// column 255 expands to the right and column 256 to the left.
    ssize_t chip_gap{2};

    /// @brief Pixels inserted at module boundaries instead of the chip gap.
    /// Unset means module boundaries receive the chip gap.
    std::optional<ModuleGaps> module_gaps{};

    /// @brief Value written to gap pixels, converted to the pixel type.
    /// Values outside the range of an integer pixel type wrap. Module gaps
    /// always receive this value.
    double fill_value{0};

    /// @brief Split the counts of the double-size pixels at chip boundaries
    /// between the pixel and its gap pixel instead of writing the fill
    /// value. Integer remainders go to one side at random. Requires a chip
    /// gap of 2.
    bool split_counts{false};

    /// @brief Seed for the random assignment of remainders. Unset means the
    /// generator is seeded from the system.
    std::optional<uint64_t> seed{};

    bool operator==(const GapPixels &other) const {
        return chip_gap == other.chip_gap && module_gaps == other.module_gaps &&
               fill_value == other.fill_value &&
               split_counts == other.split_counts && seed == other.seed;
    }
    bool operator!=(const GapPixels &other) const { return !(*this == other); }
};

namespace detail {

/**
 * @brief Gap layout along one axis in ungapped detector coordinates.
 */
struct AxisGaps {
    ssize_t chip{};
    ssize_t module{};
    ssize_t chip_gap{};
    ssize_t module_gap{};

    /// @brief Gapped coordinate of an ungapped detector coordinate
    ssize_t gapped(ssize_t coordinate) const {
        const ssize_t chips_before = coordinate / chip;
        const ssize_t modules_before = coordinate / module;
        return coordinate + chip_gap * (chips_before - modules_before) +
               module_gap * modules_before;
    }

    /// @brief Gapped size of the detector range [min, max)
    ssize_t extent(ssize_t min, ssize_t max) const {
        return max <= min ? 0 : gapped(max - 1) - gapped(min) + 1;
    }

    bool is_module_boundary(ssize_t boundary) const {
        return boundary % module == 0;
    }

    /// @brief Chip boundaries strictly inside [min, max). A boundary is the
    /// detector coordinate of the first pixel of the chip after it.
    std::vector<ssize_t> inner_boundaries(ssize_t min, ssize_t max) const {
        std::vector<ssize_t> boundaries;
        for (ssize_t boundary = (min / chip + 1) * chip; boundary < max;
             boundary += chip) {
            boundaries.push_back(boundary);
        }
        return boundaries;
    }

    /// @brief Gapped column range [first, last] of the gap at a boundary,
    /// relative to the gapped coordinate of `origin`
    std::pair<ssize_t, ssize_t> gap_range(ssize_t boundary,
                                          ssize_t origin) const {
        const ssize_t start = gapped(origin);
        return {gapped(boundary - 1) + 1 - start, gapped(boundary) - 1 - start};
    }
};

struct GapLayout {
    AxisGaps x;
    AxisGaps y;
};

/**
 * @brief Resolve the gap layout for a detector
 * @throws std::invalid_argument for detectors other than Jungfrau and Eiger
 * or for an inconsistent configuration
 */
GapLayout gap_layout(DetectorType detector, bool quad, const GapPixels &gaps);

/// @brief Gapped shape {rows, cols} of a detector coordinate ROI
std::array<ssize_t, 2> gapped_shape(const ROI &roi, const GapLayout &layout);

void validate_roi(const ROI &roi);
void validate_destination(const std::array<ssize_t, 2> &shape, const ROI &roi,
                          const GapLayout &layout);

std::mt19937_64 make_generator(std::optional<uint64_t> seed);

/// @brief Convert the fill value to the pixel type without undefined
/// behaviour for out of range values
template <typename T> T fill_as(double value) {
    if constexpr (std::is_floating_point_v<T>) {
        return static_cast<T>(value);
    } else {
        return static_cast<T>(static_cast<int64_t>(value));
    }
}

/**
 * @brief Write `value` to every gap row and gap column inside the ROI
 * @param destination gapped image of `roi`
 * @param roi detector coordinates of the destination image
 * @param layout resolved gap layout
 * @param value value written to the gap pixels
 */
template <typename T>
void fill_gap_pixels(NDView<T, 2> destination, const ROI &roi,
                     const GapLayout &layout, T value) {
    validate_destination(destination.shape(), roi, layout);
    const ssize_t rows = destination.shape(0);
    const ssize_t cols = destination.shape(1);

    for (const auto boundary : layout.x.inner_boundaries(roi.xmin, roi.xmax)) {
        const auto [first, last] = layout.x.gap_range(boundary, roi.xmin);
        for (ssize_t row = 0; row < rows; ++row) {
            for (ssize_t col = first; col <= last; ++col) {
                destination(row, col) = value;
            }
        }
    }
    for (const auto boundary : layout.y.inner_boundaries(roi.ymin, roi.ymax)) {
        const auto [first, last] = layout.y.gap_range(boundary, roi.ymin);
        for (ssize_t row = first; row <= last; ++row) {
            for (ssize_t col = 0; col < cols; ++col) {
                destination(row, col) = value;
            }
        }
    }
}

/**
 * @brief Copy a detector coordinate rectangle into its gapped position
 * @param source ungapped pixels of `source_rect`
 * @param source_rect detector coordinates of `source`, inside `roi`
 * @param destination gapped image of `roi`
 * @param roi detector coordinates of the destination image
 * @param layout resolved gap layout
 * @note Gap pixels are not written
 */
template <typename T>
void copy_with_gaps(NDView<const T, 2> source, const ROI &source_rect,
                    NDView<T, 2> destination, const ROI &roi,
                    const GapLayout &layout) {
    validate_destination(destination.shape(), roi, layout);
    validate_roi(source_rect);
    if (source_rect.xmin < roi.xmin || source_rect.xmax > roi.xmax ||
        source_rect.ymin < roi.ymin || source_rect.ymax > roi.ymax) {
        throw std::invalid_argument(LOCATION +
                                    "Source rectangle extends outside the ROI");
    }
    if (source.shape(0) != source_rect.height() ||
        source.shape(1) != source_rect.width()) {
        throw std::invalid_argument(
            LOCATION + "Source shape does not match the source rectangle");
    }
    if (source.strides()[1] != 1 || destination.strides()[1] != 1) {
        throw std::invalid_argument(
            LOCATION + "Source and destination rows must be contiguous");
    }
    if (source_rect.width() == 0 || source_rect.height() == 0) {
        return;
    }

    // contiguous column segments [begin, end) between chip boundaries
    std::vector<std::pair<ssize_t, ssize_t>> segments;
    ssize_t begin = source_rect.xmin;
    for (const auto boundary :
         layout.x.inner_boundaries(source_rect.xmin, source_rect.xmax)) {
        segments.emplace_back(begin, boundary);
        begin = boundary;
    }
    segments.emplace_back(begin, source_rect.xmax);

    const ssize_t x0 = layout.x.gapped(roi.xmin);
    const ssize_t y0 = layout.y.gapped(roi.ymin);
    for (ssize_t y = source_rect.ymin; y < source_rect.ymax; ++y) {
        const T *source_row = &source(y - source_rect.ymin, 0);
        T *destination_row = &destination(layout.y.gapped(y) - y0, 0);
        for (const auto &[first, last] : segments) {
            std::memcpy(destination_row + layout.x.gapped(first) - x0,
                        source_row + first - source_rect.xmin,
                        static_cast<size_t>(last - first) * sizeof(T));
        }
    }
}

/**
 * @brief Split the counts of the double-size pixels at every chip boundary
 * inside the ROI between the pixel and its gap pixel. Columns are split
 * first and rows second, so corner pixels end up in four parts with their
 * total preserved. Module boundaries are left untouched.
 * @param destination gapped image of `roi` after copy_with_gaps
 * @param roi detector coordinates of the destination image
 * @param layout resolved gap layout
 * @param generator source of the random remainder assignment
 */
template <typename T>
void split_edge_counts(NDView<T, 2> destination, const ROI &roi,
                       const GapLayout &layout, std::mt19937_64 &generator) {
    validate_destination(destination.shape(), roi, layout);
    if (layout.x.chip_gap != 2 || layout.y.chip_gap != 2) {
        throw std::invalid_argument(
            LOCATION + "Splitting counts requires a chip gap of 2");
    }

    auto halve = [&generator](T &pixel, T &gap) {
        if constexpr (std::is_floating_point_v<T>) {
            const T half = pixel / 2;
            pixel = half;
            gap = half;
        } else {
            T low = pixel / 2;
            T high = pixel - low;
            if (low != high && (generator() & 1u)) {
                std::swap(low, high);
            }
            pixel = low;
            gap = high;
        }
    };

    const ssize_t x0 = layout.x.gapped(roi.xmin);
    const ssize_t y0 = layout.y.gapped(roi.ymin);
    const ssize_t cols = destination.shape(1);

    // columns: every pixel row of every chip boundary that is not a module
    // boundary
    std::vector<ssize_t> chip_columns;
    for (const auto boundary : layout.x.inner_boundaries(roi.xmin, roi.xmax)) {
        if (!layout.x.is_module_boundary(boundary)) {
            chip_columns.push_back(boundary);
        }
    }
    for (ssize_t y = roi.ymin; y < roi.ymax; ++y) {
        const ssize_t row = layout.y.gapped(y) - y0;
        for (const auto boundary : chip_columns) {
            const ssize_t left = layout.x.gapped(boundary - 1) - x0;
            const ssize_t right = layout.x.gapped(boundary) - x0;
            halve(destination(row, left), destination(row, left + 1));
            halve(destination(row, right), destination(row, right - 1));
        }
    }

    // rows: every gapped column except module gaps
    std::vector<bool> module_gap_column(static_cast<size_t>(cols), false);
    for (const auto boundary : layout.x.inner_boundaries(roi.xmin, roi.xmax)) {
        if (layout.x.is_module_boundary(boundary)) {
            const auto [first, last] = layout.x.gap_range(boundary, roi.xmin);
            for (ssize_t col = first; col <= last; ++col) {
                module_gap_column[static_cast<size_t>(col)] = true;
            }
        }
    }
    for (const auto boundary : layout.y.inner_boundaries(roi.ymin, roi.ymax)) {
        if (layout.y.is_module_boundary(boundary)) {
            continue;
        }
        const ssize_t top = layout.y.gapped(boundary - 1) - y0;
        const ssize_t bottom = layout.y.gapped(boundary) - y0;
        for (ssize_t col = 0; col < cols; ++col) {
            if (module_gap_column[static_cast<size_t>(col)]) {
                continue;
            }
            halve(destination(top, col), destination(top + 1, col));
            halve(destination(bottom, col), destination(bottom - 1, col));
        }
    }
}

} // namespace detail

/**
 * @brief Shape {rows, cols} of the gapped image of a detector coordinate ROI
 * @throws std::invalid_argument for unsupported detectors or an invalid ROI
 */
std::array<ssize_t, 2> gapped_shape(const ROI &roi, DetectorType detector,
                                    bool quad, const GapPixels &gaps = {});

/**
 * @brief Insert gap pixels into an image
 * @param source ungapped image of `roi`
 * @param roi detector coordinates of `source`
 * @param detector detector type, Jungfrau or Eiger
 * @param quad true for an Eiger quad
 * @param gaps gap configuration
 * @return gapped image
 * @throws std::invalid_argument for unsupported detectors, an invalid
 * configuration or a source shape that does not match the ROI
 */
template <typename T>
NDArray<T, 2> insert_gap_pixels(NDView<const T, 2> source, const ROI &roi,
                                DetectorType detector, bool quad,
                                const GapPixels &gaps = {}) {
    const auto layout = detail::gap_layout(detector, quad, gaps);
    detail::validate_roi(roi);
    NDArray<T, 2> result(detail::gapped_shape(roi, layout));
    detail::fill_gap_pixels(result.view(), roi, layout,
                            detail::fill_as<T>(gaps.fill_value));
    detail::copy_with_gaps(source, roi, result.view(), roi, layout);
    if (gaps.split_counts) {
        auto generator = detail::make_generator(gaps.seed);
        detail::split_edge_counts(result.view(), roi, layout, generator);
    }
    return result;
}

} // namespace aare
