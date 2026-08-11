#pragma once
#include "aare/DetectorGeometry.hpp"
#include "aare/defs.hpp"
#include <algorithm>
#include <numeric>
#include <optional>
#include <vector>

namespace aare {

/**
 * @brief Merge all consecutive ROIs in a vector into a single ROI.
 * @param rois vector of ROIs to merge
 * @return vector of merged ROIs
 * @tparam horizontally_aligned true if the ROIs are horizontally aligned, false
 * otherwise
 * @tparam vertically_aligned true if the ROIs are vertically aligned, false
 * otherwise
 */
template <bool horizontally_aligned = false, bool vertically_aligned = false>
std::vector<ROI> merge_consecutive_rois(std::vector<ROI> &rois) {

    if constexpr (horizontally_aligned && vertically_aligned) {
        throw std::runtime_error(
            LOCATION + "Vector of the same ROI? Cannot merge ROIs both "
                       "horizontally and vertically at the same time.");
    }

    if (rois.empty()) {
        return {};
    }
    if (rois.size() == 1) {
        return rois;
    }

    auto merge_along_x = [](std::vector<ROI> in_rois) {
        std::sort(in_rois.begin(), in_rois.end(),
                  [](const ROI &a, const ROI &b) {
                      return (a.ymin != b.ymin) ? (a.ymin < b.ymin)
                                                : (a.xmin < b.xmin);
                  }); // N log (N)

        std::vector<ROI> merged_rois;
        merged_rois.reserve(in_rois.size());
        merged_rois.push_back(in_rois[0]);

        for (size_t i = 1; i < in_rois.size(); ++i) {
            auto &last = merged_rois.back();
            const auto &current = in_rois[i];
            if (last.ymin == current.ymin && last.ymax == current.ymax &&
                last.xmax == current.xmin) {
                // merge
                last.xmax = current.xmax;
            } else {
                merged_rois.push_back(current);
            }
        }
        return merged_rois;
    };

    auto merge_along_y = [](std::vector<ROI> in_rois) {
        std::sort(in_rois.begin(), in_rois.end(),
                  [](const ROI &a, const ROI &b) {
                      return (a.xmin != b.xmin) ? (a.xmin < b.xmin)
                                                : (a.ymin < b.ymin);
                  });

        std::vector<ROI> merged_rois;
        merged_rois.reserve(in_rois.size());
        merged_rois.push_back(in_rois[0]);

        for (size_t i = 1; i < in_rois.size(); ++i) {
            auto &last = merged_rois.back();
            const auto &current = in_rois[i];
            if (last.xmin == current.xmin && last.xmax == current.xmax &&
                last.ymax == current.ymin) {
                last.ymax = current.ymax;
            } else {
                merged_rois.push_back(current);
            }
        }
        return merged_rois;
    };

    if constexpr (horizontally_aligned) {
        return merge_along_y(rois); // one sort + one pass
    } else if constexpr (vertically_aligned) {
        return merge_along_x(rois); // one sort + one pass
    } else {
        return merge_along_y(merge_along_x(rois)); // generic case: two passes
    }
}

/**
 * @brief Check if the ROI covers the entire detector geometry
 * @param roi Region of interest
 * @param geometry Detector geometry
 * @return true if the ROI covers the entire detector geometry, false otherwise
 */
inline bool complete_ROI(const ROI &roi, const DetectorGeometry &geometry) {
    return roi.xmin == 0 &&
           roi.xmax == static_cast<ssize_t>(geometry.pixels_x()) &&
           roi.ymin == 0 &&
           roi.ymax == static_cast<ssize_t>(geometry.pixels_y());
}

inline bool complete_ROI(const std::vector<ROI> &rois,
                         const DetectorGeometry &geometry) {
    if (rois.empty() or rois.size() > 1) {
        return false;
    } else {
        return complete_ROI(rois[0], geometry);
    }
}

} // namespace aare