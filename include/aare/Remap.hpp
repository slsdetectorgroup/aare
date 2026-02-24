#pragma once

#include "aare/InclusiveROI.hpp" // IMPORTANT: Uses InclusiveROI!!!
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include "aare/RemapConfig.hpp"

#include <optional>

namespace aare::remap::model {

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

struct StrixelSensorConfig {
    // --- Sensor identity (determines multiplicator, layout, groups)
    SensorKey key;
    // std::string label;

    // --- Pixel geometry
    int cols;
    int rows;
    int guardring; // 9, 0
    BondShift bond_shift;

    // --- Strixel geometry
    int multiplicator; // 2, 3, 5, or 4
    int shift_x;       // number of “extra” square pixels (0, 1, 3, 2)
    double pitch_um;   // physical pitch (37.5 / 25 / 15 / 18.75)
    int cols_remap;
    int rows_remap;

    // --- Geometry of this strixel group *in local pixel coordinates*
    //     e.g. G1 = [10..246, 9..63]
    InclusiveROI roi_group;

    // --- Sensor placement in *module-global* coordinates
    //     Example: chip 1 = [256..511, 0..255], chip 6 = [512..757, 256..511]
    InclusiveROI roi_module;

    // --- Orientation
    // --- Rotation of the chip (Normal / Inverse)
    //     Used to mirror group ROIs and determine mod ordering.
    Rotation rotation;
    std::optional<int> chip_id; // only relevant for multiple sensors on module

  private:
    friend StrixelSensorConfig
    makeSensorConfig(SensorKey, std::optional<Rotation> user_rot,
                     std::optional<int> chip_id, BondShift);

    // dumb, private constructor! (To decouple responsibilities)
    StrixelSensorConfig(SensorKey key_, int cols_, int rows_, int guardring_,
                        BondShift bond_shift_, int multiplicator_, int shift_x_,
                        double pitch_um_, int cols_remap_, int rows_remap_,
                        InclusiveROI roi_group_, InclusiveROI roi_module_,
                        Rotation rotation_, std::optional<int> chip_id_)
        : key(key_), cols(cols_), rows(rows_), guardring(guardring_),
          bond_shift(bond_shift_), multiplicator(multiplicator_),
          shift_x(shift_x_), pitch_um(pitch_um_), cols_remap(cols_remap_),
          rows_remap(rows_remap_), roi_group(roi_group_),
          roi_module(roi_module_), rotation(rotation_), chip_id(chip_id_) {}
};

struct MappingResult {
    aare::NDArray<ssize_t, 2> order_map; // strixel coordinates
    int rows;                            // strixel coordinates
    int cols;                            // strixel coordinates
    int multiplicator;
    aare::InclusiveROI
        scd_roi_pixel; // final ROI in local pixel coordinates (smallest
                       // common denominator between receiver-ROI and
                       // strixel group with the same multiplicity)
};

} // namespace aare::remap::model

namespace aare::remap::geom {

/**
 * Align roi_user to coordinate system of roi_base
 */
aare::InclusiveROI alignROIs(InclusiveROI const &roi_user,
                             InclusiveROI const &roi_base);

/**
 * Auto-rotate based on chip_id
 */
aare::remap::model::Rotation autoRotate(int chip_id);

} // namespace aare::remap::geom

namespace aare::remap::resolve {
using namespace aare::remap::model;

aare::remap::config::GroupDescriptor const &groupDescriptor(SensorKey);
aare::remap::config::ChipGeometry chipGeometry(SensorKey);
aare::InclusiveROI moduleROI(SensorKey, std::optional<int> chip_id);

} // namespace aare::remap::resolve

namespace aare::remap::algo {
using namespace aare::remap::model;

/**
 *  Core remapping function for a single contiguous unit
 *  (Multipitch G1, G2, or G3, or halfquad).
 *
 * \param roi_user user-supplied roi in chip or quad coordinates
 * \param roi_group valid, full roi of a strixel pitch-group or contiguous
 * strixel region (e.g. halfquad) in local coordinates
 * \param multiplicator multiplicity of the strixel design; 3 (MP25, Quad), 5
 * (MP15), 4 (MP18)
 * \param rot Rotation Normal or Inverse; mods[] order reversed if rotation ==
 * Inverse.
 * \param shifty optional shift in y (in strixel map space!)
 */
MappingResult generateUnitMap(aare::InclusiveROI const &roi_user,
                              aare::InclusiveROI const &roi_group,
                              int multiplicator, Rotation rot, int shifty = 0);

/**
 * Utility to join to separately mapped core units of a Strixel Quad (bottom and
 * top half with gap-pixels in-between)
 */
MappingResult joinQuadMaps(MappingResult const &bottom,
                           MappingResult const &top, int gap_rows);

/**
 *  Public API:
 *  Generates mapping for a given region of single chip multipitch strixel
 *  sensor (G1,G2,G3) and returns only the active intersection with the ROI of
 * the JSON file.
 * \param roi_module JSON rx_ROI in module coordinates (as read from master
 * file)
 * \param key SensorKey encoding SensorTech, SensorLayout, SensorRevision
 * \param chip_id 1 or 6
 * \param rot Rotation (optional): Normal or Inverse, only supply if you know
 * what you are doing! Otherwise give std::nullopt and the rotation will be
 * automatically determined based on chip_id
 * \param bond_shift For modules like M408, where we know the bump bonding is
 * shifted in y, auto-rotates depending on chip_id or user supplied rotation,
 * x-shift is possible for completeness
 */
MappingResult
generateMPStrixelMapping(aare::InclusiveROI const &roi_user_module,
                         SensorKey key, int chip_id,
                         std::optional<Rotation> user_rot, BondShift);

/**
 *  Public API:
 *  Generates mapping for a full Quad Sensor with multiplicity 3 (25 um pitch).
 *
 * \param roi_module JSON rx_ROI in module coordinates
 * \param key SensorKey encoding SensorTech, SensorLayout, SensorRevision
 * \param rot Rotation (optional): Normal or Inverse, default is
 * Rotation::Normal (give std::nullopt)
 * \param bond_shift Included for completeness, should always be 0 for Quad
 * (give default constructed)
 */
MappingResult
generateQuadStrixelMapping(aare::InclusiveROI const &roi_user_module,
                           SensorKey key, std::optional<Rotation> user_rot,
                           BondShift);

/**
 *  Public API:
 *  Applies a given remapping rule to an input array.
 *
 * \param input Original array
 * \param order_map Rule for remapping
 * \param output Remapped array
 */
template <typename T>
void ApplyRemap(aare::NDView<T, 2> const &input,
                aare::NDArray<ssize_t, 2> const &order_map,
                aare::NDArray<T, 2> &output) {
    for (size_t row = 0; row < order_map.shape(0); ++row) {
        for (size_t col = 0; col < order_map.shape(1); ++col) {
            auto flat_index = order_map(row, col);
            if (flat_index >= 0 &&
                static_cast<size_t>(flat_index) < input.size()) {
                output(row, col) = static_cast<T>(input[flat_index]);
            } else {
                output(row, col) = static_cast<T>(0); // or nan?
            }
        }
    }
}

} // namespace aare::remap::algo

namespace aare::remap::format {
using namespace aare::remap::model;

/**
 * Helpers for printing
 */
static inline std::string toString(SensorKey);
static inline std::string toString(SensorLayout);
static inline std::string toString(SensorTech);
static inline std::string toString(SensorRevision);
static inline std::string toString(Rotation);
static inline std::string toString(StrixelSensorConfig const &c);
inline std::ostream &operator<<(std::ostream &os, StrixelSensorConfig const &c);

} // namespace aare::remap::format