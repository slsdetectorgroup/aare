#pragma once

#include <aare/defs.hpp>
#include <aare/logger.hpp>

#include <cstddef>
#include <iostream>
#include <stdexcept>

namespace aare {

/// @brief Inclusive Region of Interest (ROI) with inclusive bounds.
struct InclusiveROI {
    /// @brief start of x-coordinate.
    ssize_t xmin;
    /// @brief end of x-coordinate (inclusive).
    ssize_t xmax;
    /// @brief start of y-coordinate.
    ssize_t ymin;
    /// @brief end of y-coordinate (inclusive).
    ssize_t ymax;

    constexpr ssize_t width() const noexcept { return xmax - xmin + 1; }

    constexpr ssize_t height() const noexcept { return ymax - ymin + 1; }

    constexpr ssize_t size() const noexcept { return width() * height(); }

    [[nodiscard]] constexpr bool is_empty() const noexcept {
        return xmax < xmin || ymax < ymin;
    }

    [[nodiscard]] constexpr bool contains(ssize_t x, ssize_t y) const noexcept {
        return x >= xmin && x <= xmax && y >= ymin && y <= ymax;
    }

    [[nodiscard]] constexpr bool fits_in(ssize_t ncols,
                                         ssize_t nrows) const noexcept {
        return xmin >= 0 && ymin >= 0 && xmax < ncols && ymax < nrows;
    }

    constexpr bool operator==(InclusiveROI const &other) const noexcept {
        return xmin == other.xmin && xmax == other.xmax && ymin == other.ymin &&
               ymax == other.ymax;
    }

    static InclusiveROI emptyROI() noexcept { return {0, -1, 0, -1}; }
    // TODO (nice to have)
    // static InclusiveROI from_shape(ssize_t width, ssize_t height);
};

/**
 * @brief transorms a half-open ROI to an inclusive ROI
 * @param r half-open ROI
 * @return inclusive ROI with same physical extent
 */
inline InclusiveROI toInclusiveROI(ROI const &r) {
    return {r.xmin, r.xmax - 1, r.ymin, r.ymax - 1};
}

/** @brief
 * @brief Transforms an inclusive ROI to a half-open ROI
 * @param r inclusive ROI
 * @return half-open ROI with same physical extent
 */
inline ROI toHalfopenROI(InclusiveROI const &r) {
    return {r.xmin, r.xmax + 1, r.ymin, r.ymax + 1};
}

/***********************
 * Printing utility
 ***********************/
inline std::ostream &operator<<(std::ostream &os, InclusiveROI const &roi) {
    os << "ROI (inclusive): x=[" << roi.xmin << ", " << roi.xmax << "], y=["
       << roi.ymin << ", " << roi.ymax << "], width=" << roi.width()
       << ", height=" << roi.height() << ", pixels=" << roi.size();
    return os;
}

} // namespace aare

namespace aare::inclusiveroi::geom {

/**
 * @brief Coordinate space transforms for InclusiveROI.
 * @param r InclusiveROI to be transformed
 * @param dx translation in x direction
 * @param dy translation in y direction
 * @return InclusiveROI translated by (dx, dy)
 */
static inline InclusiveROI translate(InclusiveROI r, ssize_t dx, ssize_t dy) {
    return {r.xmin + dx, r.xmax + dx, r.ymin + dy, r.ymax + dy};
}

/**
 * @brief Convert an InclusiveROI to a local coordinate system with origin at
 * (0,0).
 * @param roi InclusiveROI to be converted
 * @return InclusiveROI in local coordinates
 */
static inline InclusiveROI to_local(InclusiveROI const &roi) {
    return {0, roi.xmax - roi.xmin, 0, roi.ymax - roi.ymin};
}

/**
 * @brief Mirror an ROI on a given y-axis (vertical axis), thereby reflecting
 * its x-coordinates horizontally
 *
 * The ROI is translated to the horizontally mirrored position while
 * preserving its size
 *
 * Example:
 * \verbatim
 *
 *        yaxis_coord                        yaxis_coord
 *  -----------|-----------            -----------|-----------
 *  |          |  r.xmax  |            |  x0p     |   x1p    |
 *  |    ******|********  |            |  ********|******    |
 *  |    *     |   roi *  |     ->     |  *mirrored roi *    |
 *  |    *     |       *  |            |  *       |     *    |
 *  |    ******|********  |            |  ********|******    |
 *  |          |          |            |          |          |
 *  -----------|-----------            -----------|-----------
 * \endverbatim
 * @param r The ROI to be mirrored
 * @param yaxis_coord The y-axis coordinate (in x) expressed in pixel
 * coordinates
 */
static inline InclusiveROI mirror_on_y(InclusiveROI r, ssize_t yaxis_coord) {
    // int x0p = (width - 1) - r.xmax;
    // int x1p = (width - 1) - r.xmin;
    int x0p = yaxis_coord * 2 - r.xmax - 1;
    int x1p = x0p + r.width() - 1;
    return {x0p, x1p, r.ymin, r.ymax};
}

/**
 * @brief Mirror an ROI on a given x-axis (horizontal axis), thereby reflecting
 * its y-coordinates vertically
 *
 * The ROI is translated to the vertically mirrored position while
 * preserving its size
 *
 * @param r The ROI to be mirrored
 * @param xaxis_coord The x-axis coordinate (in y) expressed in pixel
 * coordinates
 */
static inline InclusiveROI mirror_on_x(InclusiveROI r, ssize_t xaxis_coord) {
    // int y0p = (height - 1) - r.ymax;
    // int y1p = (height - 1) - r.ymin;
    int y0p = xaxis_coord * 2 - r.ymax - 1;
    int y1p = y0p + r.height() - 1;
    return {r.xmin, r.xmax, y0p, y1p};
}

/**
 * @brief Mirror both x- and y-coordinates.
 *
 * This is equivalent to a 180° rotation about the axes intersection.
 * @param r The ROI to be mirrored
 * @param xaxis_coord The x-axis coordinate (in y) expressed in pixel
 * coordinates
 * @param yaxis_coord The y-axis coordinate (in x) expressed in pixel
 * coordinates
 */
static inline InclusiveROI mirrorXY(InclusiveROI r, ssize_t xaxis_coord,
                                    ssize_t yaxis_coord) {
    // return {mirrorX(mirrorY(r, height), width)};
    return {mirror_on_y(mirror_on_x(r, xaxis_coord), yaxis_coord)};
}

/**
 * @brief Compute the intersection of two InclusiveROIs.
 * @param a First InclusiveROI
 * @param b Second InclusiveROI
 * @return InclusiveROI representing the intersection of a and b or empty ROI if
 * they do not intersect
 */
static inline InclusiveROI intersect(InclusiveROI const &a,
                                     InclusiveROI const &b) {
    InclusiveROI r;
    r.xmin = std::max(a.xmin, b.xmin);
    r.ymin = std::max(a.ymin, b.ymin);
    r.xmax = std::min(a.xmax, b.xmax);
    r.ymax = std::min(a.ymax, b.ymax);

    if (r.xmin > r.xmax || r.ymin > r.ymax) {
        LOG(TLogLevel::logWARNING) << "ROIs do not intersect!" << std::endl;
        return InclusiveROI::emptyROI(); // empty
    }
    return r;
}

/**
 * @brief Compute the union of two InclusiveROIs.
 * @param a First InclusiveROI
 * @param b Second InclusiveROI
 * @return InclusiveROI representing the union of a and b if they are contiguous
 * @throws std::runtime_error if the ROIs cannot be united contiguously
 */
static inline InclusiveROI unite(InclusiveROI const &a, InclusiveROI const &b) {
    // Horizontal union: same y-range
    if (a.ymin == b.ymin && a.ymax == b.ymax) {
        if (a.xmax + 1 >= b.xmin &&
            b.xmax + 1 >= a.xmin) { // overlap or adjacent
            return {std::min(a.xmin, b.xmin), std::max(a.xmax, b.xmax), a.ymin,
                    a.ymax};
        }
    }

    // Vertical union: same x-range
    if (a.xmin == b.xmin && a.xmax == b.xmax) {
        if (a.ymax + 1 >= b.ymin &&
            b.ymax + 1 >= a.ymin) { // overlap or adjacent
            return {a.xmin, a.xmax, std::min(a.ymin, b.ymin),
                    std::max(a.ymax, b.ymax)};
        }
    }

    throw std::runtime_error("ROIs cannot be united contiguously");
}

/**
 * @brief Rebase an ROI into the coordinate system of another ROI.
 * @param roi_input The ROI to be rebased
 * @param roi_base The base ROI defining the new coordinate system
 * @return InclusiveROI expressed in the coordinate system of roi_base
 *
 * The returned ROI has the same physical extent, but is expressed relative
 * to roi_base instead of the original coordinate system.
 *
 * In other words, roi_base.xmin/ymin become the new origin (0,0).
 * Example:
 *
 *   Global coordinates:
 *
 *     roi_base  = [100..199] x [50..149] \n
 *     roi_input = [120..139] x [70..89] \n
 *
 *   After rebasing:
 *
 *      roi_input = [20..39] x [20..39] \n
 */
static inline InclusiveROI rebaseROI(InclusiveROI const &roi_input,
                                     InclusiveROI const &roi_base) {
    return translate(roi_input, -roi_base.xmin, -roi_base.ymin);
}

} // namespace aare::inclusiveroi::geom