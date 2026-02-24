#include "aare/InclusiveROI.hpp" // IMPORTANT: Uses InclusiveROI!!!

namespace aare::remap::config {

/**
 * Helper structs for strong-typing
 */
struct ChipGeometry {
    int cols;
    int rows;
    int guardring;
};

struct GroupDescriptor {
    int multiplicator;
    int x_shift; // to skip additional square pixels that are not remapped
    InclusiveROI strixel_roi; // in chip-local coords (no mirroring)
    int nrows_remap;          // precomputed or computed once
    int ncols_remap;          // same
    double pitch_um;
};

// Separate mappings for the multi-pitch single-chip strixel sensor
// (small pitch in vertical)
struct SingleChipMP_iLGAD {
    static constexpr bool multi_chip = true;

    static constexpr ChipGeometry geometry{
        .cols = 256, .rows = 256, .guardring = 9};

    // Sensor placement in module coordinates:
    static constexpr InclusiveROI chip1{256, 511, 0, 255};
    static constexpr InclusiveROI chip6{512, 767, 256, 511};

    // Group descriptors:
    // Group 1: 25um pitch, groups of 3, 1 column of square pixels
    static inline const GroupDescriptor P25 = {
        .multiplicator = 3,
        .x_shift = 1,
        .strixel_roi =
            InclusiveROI{geometry.guardring + 1,
                         geometry.cols - geometry.guardring - 1,
                         geometry.guardring, (geometry.rows / 4) - 1},
        .nrows_remap = ((geometry.rows / 4) - geometry.guardring) * 3,
        .ncols_remap = (geometry.cols - (2 * geometry.guardring) - 1) / 3,
        .pitch_um = 25.0};

    // Group 2: 15um pitch, groups of 5, 3 columns of square pixels
    static inline const GroupDescriptor P15 = {
        .multiplicator = 5,
        .x_shift = 3,
        .strixel_roi =
            InclusiveROI{geometry.guardring + 3,
                         geometry.cols - geometry.guardring - 1,
                         geometry.rows / 4, (geometry.rows / 4) * 2 - 1},
        .nrows_remap = (geometry.rows / 4) * 5,
        .ncols_remap = (geometry.cols - (2 * geometry.guardring) - 3) / 5,
        .pitch_um = 15.0};

    // Group 3: 18.75um pitch, groups of 4, 2 columns of square pixels (double
    // the size of the other groups, differe in design of metal layer)
    static inline const GroupDescriptor P18 = {
        .multiplicator = 4,
        .x_shift = 2,
        .strixel_roi = InclusiveROI{geometry.guardring + 2,
                                    geometry.cols - geometry.guardring - 1,
                                    (geometry.rows / 4) * 2,
                                    geometry.rows - geometry.guardring - 1},
        .nrows_remap = ((geometry.rows / 4) * 2 - geometry.guardring) * 4,
        .ncols_remap = (geometry.cols - (2 * geometry.guardring) - 2) / 4,
        .pitch_um = 18.75};
};

struct SingleChipMP_TEW {
    static constexpr bool multi_chip = true;

    static constexpr ChipGeometry geometry{
        .cols = 256, .rows = 256, .guardring = 0};

    // Sensor placement in module coordinates:
    static constexpr InclusiveROI chip1{256, 511, 0, 255};
    static constexpr InclusiveROI chip6{512, 767, 256, 511};

    // Group descriptors:
    // Group 1: 25um pitch, groups of 3, 1 column of square pixels
    static inline const GroupDescriptor P25 = {
        .multiplicator = 3,
        .x_shift = 1,
        .strixel_roi =
            InclusiveROI{1, geometry.cols - 1, 0, (geometry.rows / 4) - 1},
        .nrows_remap = (geometry.rows / 4) * 3,
        .ncols_remap = geometry.cols / 3, // 85
        .pitch_um = 25.0};

    // Group 2: 15um pitch, groups of 5, 3 columns of square pixels
    static inline const GroupDescriptor P15 = {
        .multiplicator = 5,
        .x_shift = 1,
        .strixel_roi = InclusiveROI{1, geometry.cols - 1, geometry.rows / 4,
                                    (geometry.rows / 4) * 2 - 1},
        .nrows_remap = (geometry.rows / 4) * 5,
        .ncols_remap = geometry.cols / 5, // 51
        .pitch_um = 15.0};

    // Group 3: 18.75um pitch, groups of 4, 2 columns of square pixels (double
    // the size of the other groups, differe in design of metal layer)
    static inline const GroupDescriptor P18 = {
        .multiplicator = 4,
        .x_shift = 0,
        .strixel_roi = InclusiveROI{0, geometry.cols - 1,
                                    (geometry.rows / 4) * 2, geometry.rows - 1},
        .nrows_remap = ((geometry.rows / 4) * 2) * 4,
        .ncols_remap = geometry.cols / 4,
        .pitch_um = 18.75};
};

// Constants describing the mapping of a 25µm strixel quad sensor
// A detailed reference of the layout can be found at
// https://1drv.ms/b/c/2c9cf72ecfd75ec5/EcVe188u95wggCx7HwMAAAABH4SoINkrNCUqPnmmsUjGRg?e=TwaHzY
struct Quad_iLGAD {
    static constexpr bool multi_chip = false;

    static constexpr ChipGeometry geometry{
        .cols = 512, .rows = 512, .guardring = 9};

    // Sensor placement in module coordinates:
    static constexpr InclusiveROI coords{256, 767, 0, 511};

    // Group descriptors:
    static inline const GroupDescriptor Half = {
        .multiplicator = 3,
        .x_shift = 2,
        .strixel_roi = InclusiveROI{geometry.guardring + 1,
                                    geometry.cols - geometry.guardring - 1,
                                    geometry.guardring, geometry.rows - 2},
        .nrows_remap = (geometry.rows - 1 - geometry.guardring) * 3,
        .ncols_remap = (geometry.cols - 2 - 2 * geometry.guardring) / 3,
        .pitch_um = 25.0};
};

} // namespace aare::remap::config