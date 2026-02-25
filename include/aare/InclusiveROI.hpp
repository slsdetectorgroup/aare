#pragma once

#include <aare/defs.hpp>

#include <cstddef>
#include <iostream>
#include <stdexcept>

namespace aare {

struct InclusiveROI {
    ssize_t xmin;
    ssize_t xmax; // inclusive
    ssize_t ymin;
    ssize_t ymax; // inclusive

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

    static InclusiveROI emptyROI() noexcept { return {0, -1, 0, -1}; }
    // TODO (nice to have)
    // static InclusiveROI from_shape(ssize_t width, ssize_t height);
};

inline InclusiveROI toInclusiveROI(ROI const &r) {
    return {r.xmin, r.xmax - 1, r.ymin, r.ymax - 1};
};

inline ROI toHalfopenROI(InclusiveROI const& r) {
  return {r.xmin, r.xmax + 1, r.ymin, r.ymax + 1};
}

/***********************
 * Printint utility
 ***********************/
inline std::ostream &operator<<(std::ostream &os, InclusiveROI const &roi) {
    os << "ROI (inclusive): x=[" << roi.xmin << ", " << roi.xmax << "], y=["
       << roi.ymin << ", " << roi.ymax << "], width=" << roi.width()
       << ", height=" << roi.height() << ", pixels=" << roi.size();
    return os;
};

} // namespace aare

namespace aare::inclusiveroi::geom {

// coordinate space transforms
static inline InclusiveROI translate(InclusiveROI r, ssize_t dx, ssize_t dy) {
    return {r.xmin + dx, r.xmax + dx, r.ymin + dy, r.ymax + dy};
}
static inline InclusiveROI to_local(InclusiveROI const &roi) {
    return {0, roi.xmax - roi.xmin, 0, roi.ymax - roi.ymin};
}

// mirroring (with respect to external grid given by width and height)
static inline InclusiveROI mirrorX(InclusiveROI r, ssize_t width) {
    int x0p = (width - 1) - r.xmax;
    int x1p = (width - 1) - r.xmin;
    return {x0p, x1p, r.ymin, r.ymax};
}
static inline InclusiveROI mirrorY(InclusiveROI r, ssize_t height) {
    int y0p = (height - 1) - r.ymax;
    int y1p = (height - 1) - r.ymin;
    return {r.xmin, r.xmax, y0p, y1p};
}
static inline InclusiveROI mirrorXY(InclusiveROI r, ssize_t width,
                                    ssize_t height) {
    return {mirrorX(mirrorY(r, height), width)};
}

// intersection / union
static inline InclusiveROI intersect(InclusiveROI const &a,
                                     InclusiveROI const &b) {
    InclusiveROI r;
    r.xmin = std::max(a.xmin, b.xmin);
    r.ymin = std::max(a.ymin, b.ymin);
    r.xmax = std::min(a.xmax, b.xmax);
    r.ymax = std::min(a.ymax, b.ymax);

    if (r.xmin > r.xmax || r.ymin > r.ymax) {
        std::cout << "WARNING: ROIs do not intersect!" << std::endl;
        return InclusiveROI::emptyROI(); // empty
    }
    return r;
}

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

} // namespace aare::inclusiveroi::geom