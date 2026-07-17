#include "aare/InclusiveROI.hpp" // IMPORTANT: Uses InclusiveROI!!!
#include "aare/RemapDefs.hpp"

namespace aare::remap::config::jungfrau {

/************************************
 * Default strixel geometries
 ************************************/
inline constexpr defs::GroupStrixelGeometry StrxP25{.multiplicity = 3,
                                                    .pitch_um = 25.0};
inline constexpr defs::GroupStrixelGeometry StrxP15{.multiplicity = 5,
                                                    .pitch_um = 15.0};
inline constexpr defs::GroupStrixelGeometry StrxP18{.multiplicity = 4,
                                                    .pitch_um = 18.75};
inline constexpr defs::GroupStrixelGeometry StrxP37{.multiplicity = 2,
                                                    .pitch_um = 37.5};

/************************************
 * Default sensor placements
 ************************************/
inline constexpr defs::SensorModulePlacement Chip1{
    .placement_on_module{256, 511, 0, 255},
    .rotation = defs::Rotation::Identity};
inline constexpr defs::SensorModulePlacement Chip6{
    .placement_on_module{512, 767, 256, 511},
    .rotation = defs::Rotation::Rotate180};
inline constexpr defs::SensorModulePlacement Quad{
    .placement_on_module{256, 767, 0, 511},
    .rotation = defs::Rotation::Identity};

/************************************
 * Single chip, multi-pitch, iLGAD
 ************************************/
inline constexpr defs::SensorPixelGeometry SingleChipMP_iLGAD_pix{
    .num_pix_x = 256, .num_pix_y = 256, .guardring = {.x = 9, .y = 9}};

inline constexpr defs::GroupConfig SingleChipMP_iLGAD_P25{
    .strixel = StrxP25,
    .placement_on_sensor = {SingleChipMP_iLGAD_pix.guardring.x + 1, // 10
                            SingleChipMP_iLGAD_pix.num_pix_x -
                                SingleChipMP_iLGAD_pix.guardring.x - 1,   // 246
                            SingleChipMP_iLGAD_pix.guardring.y,           // 9
                            (SingleChipMP_iLGAD_pix.num_pix_y / 4) - 1}}; // 63
/* Number of strixel columns: 79
 * Number of strixel rows: 165
 ********************************/

inline constexpr defs::GroupConfig SingleChipMP_iLGAD_P15{
    .strixel = StrxP15,
    .placement_on_sensor = {SingleChipMP_iLGAD_pix.guardring.x + 3, // 12
                            SingleChipMP_iLGAD_pix.num_pix_x -
                                SingleChipMP_iLGAD_pix.guardring.x - 1, // 246
                            SingleChipMP_iLGAD_pix.num_pix_y / 4,       // 64
                            (SingleChipMP_iLGAD_pix.num_pix_y / 4) * 2 -
                                1}}; // 127
/* Number of strixel columns: 47
 * Number of strixel rows: 320
 ********************************/

inline constexpr defs::GroupConfig SingleChipMP_iLGAD_P18{
    .strixel = StrxP18,
    .placement_on_sensor = {SingleChipMP_iLGAD_pix.guardring.x + 2, // 11
                            SingleChipMP_iLGAD_pix.num_pix_x -
                                SingleChipMP_iLGAD_pix.guardring.x - 1, // 246
                            (SingleChipMP_iLGAD_pix.num_pix_y / 4) * 2, // 128
                            SingleChipMP_iLGAD_pix.num_pix_y -
                                SingleChipMP_iLGAD_pix.guardring.y - 1}}; // 246
/* Number of strixel columns: 59
 * Number of strixel rows: 476
 ********************************/

/************************************
 * Single chip, multi-pitch, TEW
 ************************************/
inline constexpr defs::SensorPixelGeometry SingleChipMP_TEW_pix{
    .num_pix_x = 256, .num_pix_y = 256, .guardring = {.x = 0, .y = 0}};

inline constexpr defs::GroupConfig SingleChipMP_TEW_P25{
    .strixel = StrxP25,
    .placement_on_sensor = {SingleChipMP_TEW_pix.guardring.x + 1, // 1
                            SingleChipMP_TEW_pix.num_pix_x -
                                SingleChipMP_TEW_pix.guardring.x - 1,   // 255
                            SingleChipMP_TEW_pix.guardring.y,           // 0
                            (SingleChipMP_TEW_pix.num_pix_y / 4) - 1}}; // 63
/* Number of strixel columns: 85
 * Number of strixel rows: 192
 ********************************/

inline constexpr defs::GroupConfig SingleChipMP_TEW_P15{
    .strixel = StrxP15,
    .placement_on_sensor = {SingleChipMP_TEW_pix.guardring.x + 1, // 1
                            SingleChipMP_TEW_pix.num_pix_x -
                                SingleChipMP_TEW_pix.guardring.x - 1, // 255
                            SingleChipMP_TEW_pix.num_pix_y / 4,       // 64
                            (SingleChipMP_TEW_pix.num_pix_y / 4) * 2 -
                                1}}; // 127
/* Number of strixel columns: 51
 * Number of strixel rows: 320
 ********************************/

inline constexpr defs::GroupConfig SingleChipMP_TEW_P18{
    .strixel = StrxP18,
    .placement_on_sensor = {SingleChipMP_TEW_pix.guardring.x, // 0
                            SingleChipMP_TEW_pix.num_pix_x -
                                SingleChipMP_TEW_pix.guardring.x - 1, // 255
                            (SingleChipMP_TEW_pix.num_pix_y / 4) * 2, // 128
                            SingleChipMP_TEW_pix.num_pix_y -
                                SingleChipMP_TEW_pix.guardring.y - 1}}; // 255
/* Number of strixel columns: 64
 * Number of strixel rows: 512
 ********************************/

/************************************
 * Quad, 25 um, iLGAD
 ************************************/
inline constexpr defs::SensorPixelGeometry Quad_iLGAD_pix{
    .num_pix_x = 512, .num_pix_y = 512, .guardring = {.x = 9, .y = 9}};

inline constexpr defs::GroupConfig Quad_iLGAD_bottomhalf{
    .strixel = StrxP25,
    .placement_on_sensor = {Quad_iLGAD_pix.guardring.x + 2, // 11
                            Quad_iLGAD_pix.num_pix_x -
                                Quad_iLGAD_pix.guardring.x - 1,   // 502
                            Quad_iLGAD_pix.guardring.y,           // 9
                            (Quad_iLGAD_pix.num_pix_y / 2) - 2}}; // 254

inline constexpr defs::GroupConfig Quad_iLGAD_tophalf{
    .strixel = StrxP25,
    .routing = {defs::ModuloOrdering::Reverse},
    // Adapt placement to be correct!
    .placement_on_sensor = {
        Quad_iLGAD_pix.guardring.x + 2,                              // 11
        Quad_iLGAD_pix.num_pix_x - Quad_iLGAD_pix.guardring.x - 1,   // 502
        Quad_iLGAD_pix.num_pix_y / 2 + 1,                            // 257
        Quad_iLGAD_pix.num_pix_y - Quad_iLGAD_pix.guardring.y - 1}}; // 502
} // namespace aare::remap::config::jungfrau