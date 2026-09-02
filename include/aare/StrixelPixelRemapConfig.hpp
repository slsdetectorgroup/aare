#include "aare/StrixelPixelRemapDefs.hpp"

namespace aare::remap::config::jungfrau {

// Note: designated initializers (commented) only in C++20

/************************************
 * Default strixel geometries
 ************************************/

/// @brief Strixel geometry for 25 µm pitch strixels on iLGAD sensors
/// (multiplicity = 3)
inline constexpr defs::GroupStrixelGeometry StrxP25{3,     // .multiplicity = 3,
                                                    25.0}; //.pitch_um = 25.0};
/// @brief Strixel geometry for 15 µm pitch strixels on iLGAD sensors
/// (multiplicity = 5)
inline constexpr defs::GroupStrixelGeometry StrxP15{5,     //.multiplicity = 5,
                                                    15.0}; //.pitch_um = 15.0};
/// @brief Strixel geometry for 18.75 µm pitch strixels on iLGAD sensors
/// (multiplicity = 4)
inline constexpr defs::GroupStrixelGeometry StrxP18{
    4,      //.multiplicity = 4,
    18.75}; //.pitch_um = 18.75};

/// @brief Strixel geometry for 37.5 µm pitch strixels on iLGAD sensors
/// (multiplicity = 2)
inline constexpr defs::GroupStrixelGeometry StrxP37{2,     //.multiplicity = 2,
                                                    37.5}; //.pitch_um = 37.5};

/************************************
 * Default sensor placements
 ************************************/

/// @brief Placement of the 2x2cm iLGAD sensor on the second chip (Chip1) of the
/// Jungfrau module, with no rotation applied.
inline constexpr defs::SensorModulePlacement Chip1{
    {256, 511, 0, 255},        //.placement_on_module{256, 511, 0, 255},
    defs::Rotation::Identity}; //.rotation = defs::Rotation::Identity};

/// @brief Placement of the 2x2cm iLGAD sensor on the seventh chip (Chip6) of
/// the Jungfrau module, with a 180-degree rotation applied.
inline constexpr defs::SensorModulePlacement Chip6{
    {512, 767, 256, 511},       //.placement_on_module{512, 767, 256, 511},
    defs::Rotation::Rotate180}; //.rotation = defs::Rotation::Rotate180};

/// @brief Placement of the 4x4cm iLGAD sensor on the quad
/// (Chip1+Chip2+Chip5+Chip6) of the Jungfrau module, with no rotation applied.
inline constexpr defs::SensorModulePlacement Quad{
    {256, 767, 0, 511},        //.placement_on_module{256, 767, 0, 511},
    defs::Rotation::Identity}; //.rotation = defs::Rotation::Identity};

/************************************
 * Single chip, multi-pitch, iLGAD
 ************************************/

/// @brief Pixel geometry of the 2x2 cm iLGAD sensor
inline constexpr defs::SensorPixelGeometry SingleChipMP_iLGAD_pix{
    256, 256, {9, 9}};
// .num_pix_x = 256, .num_pix_y = 256, .guardring = {.x = 9, .y = 9}};

/// @brief Strixel group of 25 µm pitch strixels on the 2x2 cm iLGAD sensor
inline constexpr defs::GroupConfig SingleChipMP_iLGAD_P25{
    StrxP25, //.strixel = StrxP25,
    {defs::ModuloOrdering::Forward},
    // .placement_on_sensor
    {SingleChipMP_iLGAD_pix.guardring.x + 1, // 10
     SingleChipMP_iLGAD_pix.num_pix_x - SingleChipMP_iLGAD_pix.guardring.x -
         1,                                        // 246
     SingleChipMP_iLGAD_pix.guardring.y,           // 9
     (SingleChipMP_iLGAD_pix.num_pix_y / 4) - 1}}; // 63
/* Number of strixel columns: 79
 * Number of strixel rows: 165
 ********************************/

/// @brief Strixel group of 15 µm pitch strixels on the 2x2 cm iLGAD sensor
inline constexpr defs::GroupConfig SingleChipMP_iLGAD_P15{
    StrxP15, // .strixel
    {defs::ModuloOrdering::Forward},
    //.placement_on_sensor
    {SingleChipMP_iLGAD_pix.guardring.x + 3, // 12
     SingleChipMP_iLGAD_pix.num_pix_x - SingleChipMP_iLGAD_pix.guardring.x -
         1,                                            // 246
     SingleChipMP_iLGAD_pix.num_pix_y / 4,             // 64
     (SingleChipMP_iLGAD_pix.num_pix_y / 4) * 2 - 1}}; // 127
/* Number of strixel columns: 47
 * Number of strixel rows: 320
 ********************************/

/// @brief Strixel group of 18.75 µm pitch strixels on the 2x2 cm iLGAD sensor
inline constexpr defs::GroupConfig SingleChipMP_iLGAD_P18{
    StrxP18, // .strixel =
    {defs::ModuloOrdering::Forward},
    // .placement_on_sensor
    {SingleChipMP_iLGAD_pix.guardring.x + 2, // 11
     SingleChipMP_iLGAD_pix.num_pix_x - SingleChipMP_iLGAD_pix.guardring.x -
         1,                                      // 246
     (SingleChipMP_iLGAD_pix.num_pix_y / 4) * 2, // 128
     SingleChipMP_iLGAD_pix.num_pix_y - SingleChipMP_iLGAD_pix.guardring.y -
         1}}; // 246
/* Number of strixel columns: 59
 * Number of strixel rows: 476
 ********************************/

/// @brief Sensor configuration of the 2x2 cm iLGAD sensor with all strixel
/// groups
inline constexpr defs::SensorConfig<3> SingleChipMP_iLGAD{
    SingleChipMP_iLGAD_pix, //.pixel =
    // .group_configs =
    {SingleChipMP_iLGAD_P25, SingleChipMP_iLGAD_P15, SingleChipMP_iLGAD_P18}};

/************************************
 * Single chip, multi-pitch, Thin Entrance Window (TEW)
 ************************************/
/// @brief Pixel geometry of the 2x2 cm TEW sensors with no guard ring.
inline constexpr defs::SensorPixelGeometry SingleChipMP_TEW_pix{
    256, 256, {0, 0}};
// .num_pix_x = 256, .num_pix_y = 256, .guardring = {.x = 0, .y = 0}};

/// @brief Strixel group of 25 µm pitch strixels on the 2x2 cm TEW sensor
inline constexpr defs::GroupConfig SingleChipMP_TEW_P25{
    StrxP25, //.strixel =
    {defs::ModuloOrdering::Forward},
    // .placement_on_sensor
    {SingleChipMP_TEW_pix.guardring.x + 1, // 1
     SingleChipMP_TEW_pix.num_pix_x - SingleChipMP_TEW_pix.guardring.x -
         1,                                      // 255
     SingleChipMP_TEW_pix.guardring.y,           // 0
     (SingleChipMP_TEW_pix.num_pix_y / 4) - 1}}; // 63
/* Number of strixel columns: 85
 * Number of strixel rows: 192
 ********************************/

/// @brief Strixel group of 15 µm pitch strixels on the 2x2 cm TEW sensor
inline constexpr defs::GroupConfig SingleChipMP_TEW_P15{
    StrxP15, //.strixel =
    {defs::ModuloOrdering::Forward},
    // .placement_on_sensor
    {SingleChipMP_TEW_pix.guardring.x + 1, // 1
     SingleChipMP_TEW_pix.num_pix_x - SingleChipMP_TEW_pix.guardring.x -
         1,                                          // 255
     SingleChipMP_TEW_pix.num_pix_y / 4,             // 64
     (SingleChipMP_TEW_pix.num_pix_y / 4) * 2 - 1}}; // 127
/* Number of strixel columns: 51
 * Number of strixel rows: 320
 ********************************/

/// @brief Strixel group of 18.75 µm pitch strixels on the 2x2 cm TEW sensor
inline constexpr defs::GroupConfig SingleChipMP_TEW_P18{
    StrxP18, //.strixel =
    {defs::ModuloOrdering::Forward},
    // .placement_on_sensor
    {SingleChipMP_TEW_pix.guardring.x, // 0
     SingleChipMP_TEW_pix.num_pix_x - SingleChipMP_TEW_pix.guardring.x -
         1,                                    // 255
     (SingleChipMP_TEW_pix.num_pix_y / 4) * 2, // 128
     SingleChipMP_TEW_pix.num_pix_y - SingleChipMP_TEW_pix.guardring.y -
         1}}; // 255
/* Number of strixel columns: 64
 * Number of strixel rows: 512
 ********************************/

/// @brief Sensor configuration of the 2x2 cm TEW sensor with all strixel groups
inline constexpr defs::SensorConfig<3> SingleChipMP_TEW{
    SingleChipMP_TEW_pix, //.pixel =
    // .group_configs
    {SingleChipMP_TEW_P25, SingleChipMP_TEW_P15, SingleChipMP_TEW_P18}};

/************************************
 * Quad, 25 um, iLGAD
 ************************************/

/// @brief Pixel geometry of the 4x4 cm iLGAD sensor
inline constexpr defs::SensorPixelGeometry Quad_iLGAD_pix{512, 512, {9, 9}};
// .num_pix_x = 512, .num_pix_y = 512, .guardring = {.x = 9, .y = 9}};

inline constexpr size_t Quad_iLGAD_strixel_gap_rows = 12;

/// @brief Strixel group of 25 µm pitch strixels located on the bottom half of
/// 4x4 cm iLGAD sensor
inline constexpr defs::GroupConfig Quad_iLGAD_bottomhalf{
    StrxP25, //.strixel
    {defs::ModuloOrdering::Forward},
    // .placement_on_sensor
    {Quad_iLGAD_pix.guardring.x + 2,                            // 11
     Quad_iLGAD_pix.num_pix_x - Quad_iLGAD_pix.guardring.x - 1, // 502
     Quad_iLGAD_pix.guardring.y,                                // 9
     (Quad_iLGAD_pix.num_pix_y / 2) - 2}};                      // 254

/// @brief Strixel group of 25 µm pitch strixels located on the top half of 4x4
/// cm iLGAD sensor
inline constexpr defs::GroupConfig Quad_iLGAD_tophalf{
    StrxP25,                         //.strixel =
    {defs::ModuloOrdering::Reverse}, //.routing =
    // .placement_on_sensor
    {Quad_iLGAD_pix.guardring.x + 2,                              // 11
     Quad_iLGAD_pix.num_pix_x - Quad_iLGAD_pix.guardring.x - 1,   // 502
     Quad_iLGAD_pix.num_pix_y / 2 + 1,                            // 257
     Quad_iLGAD_pix.num_pix_y - Quad_iLGAD_pix.guardring.y - 1}}; // 502

/// @brief Sensor configuration of the 4x4 cm iLGAD sensor with all strixel
/// groups
inline constexpr defs::SensorConfig<2> Quad_iLGAD{
    Quad_iLGAD_pix, //.pixel =
    // .group_configs
    {Quad_iLGAD_bottomhalf, Quad_iLGAD_tophalf}};
} // namespace aare::remap::config::jungfrau