#include "aare/InclusiveROI.hpp" // IMPORTANT: Uses InclusiveROI!!!
#include "aare/RemapDefs.hpp"

namespace aare::remap::config::jungfrau {

/************************************
 * Default strixel geometries
 ************************************/
constexpr defs::SensorStrixelGeometry StrxP25{.multiplicity = 3,
                                              .pitch_um = 25.0};
constexpr defs::SensorStrixelGeometry StrxP15{.multiplicity = 5,
                                              .pitch_um = 15.0};
constexpr defs::SensorStrixelGeometry StrxP18{.multiplicity = 4,
                                              .pitch_um = 18.75};
constexpr defs::SensorStrixelGeometry StrxP37{.multiplicity = 2,
                                              .pitch_um = 37.5};

/************************************
 * Default sensor placements
 ************************************/
constexpr defs::SensorPlacement Chip1{.placement_on_module{256, 511, 0, 255},
                                      .rotation = defs::Rotation::Normal};
constexpr defs::SensorPlacement Chip6{.placement_on_module{512, 767, 256, 511},
                                      .rotation = defs::Rotation::Inverse};
constexpr defs::SensorPlacement Quad{.placement_on_module{256, 767, 0, 511},
                                     .rotation = defs::Rotation::Normal};

/************************************
 * Single chip, multi-pitch, iLGAD
 ************************************/
constexpr defs::SensorPixelGeometry SingelChipMP_iLGAD_pix{
    .num_pix_x = 256, .num_pix_y = 256, .guardring = {.x = 9, .y = 9}};

constexpr defs::SensorGroupConfig SingleChipMP_iLGAD_P25{
    .pixel = SingelChipMP_iLGAD_pix,
    .strixel = StrxP25,
    .placement_on_sensor = {SingelChipMP_iLGAD_pix.guardring.x + 1,
                            SingelChipMP_iLGAD_pix.num_pix_x -
                                SingelChipMP_iLGAD_pix.guardring.x - 1,
                            SingelChipMP_iLGAD_pix.guardring.y,
                            (SingelChipMP_iLGAD_pix.num_pix_y / 4) - 1}};

constexpr defs::SensorGroupConfig SingleChipMP_iLGAD_P15{
    .pixel = SingelChipMP_iLGAD_pix,
    .strixel = StrxP15,
    .placement_on_sensor = {SingelChipMP_iLGAD_pix.guardring.x + 3,
                            SingelChipMP_iLGAD_pix.num_pix_x -
                                SingelChipMP_iLGAD_pix.guardring.x - 1,
                            SingelChipMP_iLGAD_pix.num_pix_y / 4,
                            (SingelChipMP_iLGAD_pix.num_pix_y / 4) * 2 - 1}};

constexpr defs::SensorGroupConfig SingleChipMP_iLGAD_P18{
    .pixel = SingelChipMP_iLGAD_pix,
    .strixel = StrxP18,
    .placement_on_sensor = {SingelChipMP_iLGAD_pix.guardring.x + 2,
                            SingelChipMP_iLGAD_pix.num_pix_x -
                                SingelChipMP_iLGAD_pix.guardring.x - 1,
                            (SingelChipMP_iLGAD_pix.num_pix_y / 4) * 2,
                            SingelChipMP_iLGAD_pix.num_pix_y -
                                SingelChipMP_iLGAD_pix.guardring.y - 1}};

/************************************
 * Single chip, multi-pitch, TEW
 ************************************/
constexpr defs::SensorPixelGeometry SingelChipMP_TEW_pix{
    .num_pix_x = 256, .num_pix_y = 256, .guardring = {.x = 0, .y = 0}};

constexpr defs::SensorGroupConfig SingleChipMP_TEW_P25{
    .pixel = SingelChipMP_TEW_pix,
    .strixel = StrxP25,
    .placement_on_sensor = {SingelChipMP_TEW_pix.guardring.x + 1,
                            SingelChipMP_TEW_pix.num_pix_x -
                                SingelChipMP_TEW_pix.guardring.x - 1,
                            SingelChipMP_TEW_pix.guardring.y,
                            (SingelChipMP_TEW_pix.num_pix_y / 4) - 1}};

constexpr defs::SensorGroupConfig SingleChipMP_TEW_P15{
    .pixel = SingelChipMP_TEW_pix,
    .strixel = StrxP15,
    .placement_on_sensor = {SingelChipMP_TEW_pix.guardring.x + 1,
                            SingelChipMP_TEW_pix.num_pix_x -
                                SingelChipMP_TEW_pix.guardring.x - 1,
                            SingelChipMP_TEW_pix.num_pix_y / 4,
                            (SingelChipMP_TEW_pix.num_pix_y / 4) * 2 - 1}};

constexpr defs::SensorGroupConfig SingleChipMP_TEW_P18{
    .pixel = SingelChipMP_TEW_pix,
    .strixel = StrxP18,
    .placement_on_sensor = {
        SingelChipMP_TEW_pix.guardring.x,
        SingelChipMP_TEW_pix.num_pix_x - SingelChipMP_TEW_pix.guardring.x - 1,
        (SingelChipMP_TEW_pix.num_pix_y / 4) * 2,
        SingelChipMP_TEW_pix.num_pix_y - SingelChipMP_TEW_pix.guardring.y - 1}};

/************************************
 * Quad, 25 um, iLGAD
 ************************************/
constexpr defs::SensorPixelGeometry Quad_iLGAD_pix{
    .num_pix_x = 512, .num_pix_y = 512, .guardring = {.x = 9, .y = 9}};

constexpr defs::SensorGroupConfig Quad_iLGAD_half{
    .pixel = Quad_iLGAD_pix,
    .strixel = StrxP25,
    .placement_on_sensor = {
        Quad_iLGAD_pix.guardring.x + 2,
        Quad_iLGAD_pix.num_pix_x - Quad_iLGAD_pix.guardring.x - 1,
        Quad_iLGAD_pix.guardring.y, (Quad_iLGAD_pix.num_pix_y / 2) - 1}};
} // namespace aare::remap::config::jungfrau

/**************************************
 * Legacy for reference, to be deleted
 **************************************/
namespace aare::remap::legacy {

enum class SensorTech : int { iLGAD, TEW };

// This is dummy for now because it is not yet decided how best to depict
// the differences in batches R&D, production and year in a scalable way
// with unified naming conventions
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
// Normal means lower part of sensor (as in GDS) aligns with lower part of
// HDI
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

    // Group 3: 18.75um pitch, groups of 4, 2 columns of square pixels
    // (double the size of the other groups, differe in design of metal
    // layer)
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

    // Group 3: 18.75um pitch, groups of 4, 2 columns of square pixels
    // (double the size of the other groups, differe in design of metal
    // layer)
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
            InclusiveROI{chip.guardring + 2, chip.cols - chip.guardring - 1,
                         chip.guardring, chip.rows / 2},
        .nrows_remap = (chip.rows - 1 - chip.guardring) * 3,
        .ncols_remap = (chip.cols - 2 - 2 * chip.guardring) / 3,
        .pitch_um = 25.0};
};

} // namespace aare::remap::legacy