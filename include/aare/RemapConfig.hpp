#include "aare/InclusiveROI.hpp" // IMPORTANT: Uses InclusiveROI!!!

namespace aare::remap::defs {

enum class SensorTech : int { iLGAD, TEW };

// This is dummy for now because it is not yet decided how best to depict the
// differences in batches R&D, production and year in a scalable way with
// unified naming conventions
enum class SensorRevision : int { RevA, RevB, RevC };

enum class SensorLayout : int {
    Halfmodule, // not implemented yet!
    Quad,
    DoubleChip, // not implemented yet (new batch)!
    SingleMP37, // not implemented yet (TEW 2024)
    SingleMP25,
    SingleMP18,
    SingleMP15
};

// Orientation of the sensor with respect to the ASICs / the HDI
// Normal means lower part of sensor (as in GDS) aligns with lower part of HDI
enum class Rotation : int { Normal = 0, Inverse = 1 };

struct SensorKey {
    SensorTech tech;
    SensorRevision rev = SensorRevision::RevA;
    SensorLayout layout;
};

struct BondShift {
    int x = 0;
    int y = 0;
};

/**
 * Helper structs for strong-typing
 */
struct ChipGeometry {
    int cols;
    int rows;
    int guardring;
};

struct StrixelGeometry {
    int multiplicator; // 2, 3, 5, or 4
    int x_shift;       // to skip additional square pixels that are not remapped
    InclusiveROI strixel_roi; // in chip-local coords (no mirroring)
    int nrows_remap;          // precomputed or computed once
    int ncols_remap;          // same
    double pitch_um;          // physical pitch (37.5 / 25 / 15 / 18.75)
};

// Separate mappings for the multi-pitch single-chip strixel sensor
// (small pitch in vertical)
struct SingleChipMP_iLGAD {
    static constexpr bool multi_chip = true;

    static constexpr ChipGeometry chip{
        .cols = 256, .rows = 256, .guardring = 9};

    // Sensor placement in module coordinates:
    static constexpr InclusiveROI chip1{256, 511, 0, 255};
    static constexpr InclusiveROI chip6{512, 767, 256, 511};

    // Group descriptors:
    // Group 1: 25um pitch, groups of 3, 1 column of square pixels
    static inline const StrixelGeometry P25 = {
        .multiplicator = 3,
        .x_shift = 1,
        .strixel_roi =
            InclusiveROI{chip.guardring + 1, chip.cols - chip.guardring - 1,
                         chip.guardring, (chip.rows / 4) - 1},
        .nrows_remap = ((chip.rows / 4) - chip.guardring) * 3,
        .ncols_remap = (chip.cols - (2 * chip.guardring) - 1) / 3,
        .pitch_um = 25.0};

    // Group 2: 15um pitch, groups of 5, 3 columns of square pixels
    static inline const StrixelGeometry P15 = {
        .multiplicator = 5,
        .x_shift = 3,
        .strixel_roi =
            InclusiveROI{chip.guardring + 3, chip.cols - chip.guardring - 1,
                         chip.rows / 4, (chip.rows / 4) * 2 - 1},
        .nrows_remap = (chip.rows / 4) * 5,
        .ncols_remap = (chip.cols - (2 * chip.guardring) - 3) / 5,
        .pitch_um = 15.0};

    // Group 3: 18.75um pitch, groups of 4, 2 columns of square pixels (double
    // the size of the other groups, differe in design of metal layer)
    static inline const StrixelGeometry P18 = {
        .multiplicator = 4,
        .x_shift = 2,
        .strixel_roi =
            InclusiveROI{chip.guardring + 2, chip.cols - chip.guardring - 1,
                         (chip.rows / 4) * 2, chip.rows - chip.guardring - 1},
        .nrows_remap = ((chip.rows / 4) * 2 - chip.guardring) * 4,
        .ncols_remap = (chip.cols - (2 * chip.guardring) - 2) / 4,
        .pitch_um = 18.75};
};

struct SingleChipMP_TEW {
    static constexpr bool multi_chip = true;

    static constexpr ChipGeometry chip{
        .cols = 256, .rows = 256, .guardring = 0};

    // Sensor placement in module coordinates:
    static constexpr InclusiveROI chip1{256, 511, 0, 255};
    static constexpr InclusiveROI chip6{512, 767, 256, 511};

    // Group descriptors:
    // Group 1: 25um pitch, groups of 3, 1 column of square pixels
    static inline const StrixelGeometry P25 = {
        .multiplicator = 3,
        .x_shift = 1,
        .strixel_roi = InclusiveROI{1, chip.cols - 1, 0, (chip.rows / 4) - 1},
        .nrows_remap = (chip.rows / 4) * 3,
        .ncols_remap = chip.cols / 3, // 85
        .pitch_um = 25.0};

    // Group 2: 15um pitch, groups of 5, 3 columns of square pixels
    static inline const StrixelGeometry P15 = {
        .multiplicator = 5,
        .x_shift = 1,
        .strixel_roi = InclusiveROI{1, chip.cols - 1, chip.rows / 4,
                                    (chip.rows / 4) * 2 - 1},
        .nrows_remap = (chip.rows / 4) * 5,
        .ncols_remap = chip.cols / 5, // 51
        .pitch_um = 15.0};

    // Group 3: 18.75um pitch, groups of 4, 2 columns of square pixels (double
    // the size of the other groups, differe in design of metal layer)
    static inline const StrixelGeometry P18 = {
        .multiplicator = 4,
        .x_shift = 0,
        .strixel_roi =
            InclusiveROI{0, chip.cols - 1, (chip.rows / 4) * 2, chip.rows - 1},
        .nrows_remap = ((chip.rows / 4) * 2) * 4,
        .ncols_remap = chip.cols / 4,
        .pitch_um = 18.75};
};

// Constants describing the mapping of a 25µm strixel quad sensor
// A detailed reference of the layout can be found at
// https://1drv.ms/b/c/2c9cf72ecfd75ec5/EcVe188u95wggCx7HwMAAAABH4SoINkrNCUqPnmmsUjGRg?e=TwaHzY
struct Quad_iLGAD {
    static constexpr bool multi_chip = false;

    static constexpr ChipGeometry chip{
        .cols = 512, .rows = 512, .guardring = 9};

    // Sensor placement in module coordinates:
    static constexpr InclusiveROI coords{256, 767, 0, 511};

    // Group descriptors:
    static inline const StrixelGeometry Half = {
        .multiplicator = 3,
        .x_shift = 2,
        .strixel_roi =
            InclusiveROI{chip.guardring + 1, chip.cols - chip.guardring - 1,
                         chip.guardring, chip.rows - 2},
        .nrows_remap = (chip.rows - 1 - chip.guardring) * 3,
        .ncols_remap = (chip.cols - 2 - 2 * chip.guardring) / 3,
        .pitch_um = 25.0};
};

} // namespace aare::remap::defs