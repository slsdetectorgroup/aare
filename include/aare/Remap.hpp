#pragma once

#include "aare/InclusiveROI.hpp" // IMPORTANT: Uses InclusiveROI!!!
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include "aare/RemapConfig.hpp"

#include <optional>

namespace aare::remap::model {

struct StrixelSensorConfig {
    // --- Sensor identity (determines multiplicator, layout, groups)
    legacy::SensorKey key;
    // std::string label;

    // --- Pixel geometry
    legacy::ChipGeometry chip_geometry;
    defs::BondShift bond_shift;

    // --- Strixel geometry
    legacy::StrixelGeometry strixel_geometry;

    // --- Geometry of this strixel group *in local pixel coordinates*
    //     e.g. G1 = [10..246, 9..63]
    // InclusiveROI roi_group; // now contained in StrixelGeometry

    // --- Sensor placement in *module-global* coordinates
    //     Example: chip 1 = [256..511, 0..255], chip 6 = [512..757, 256..511]
    //     This should be externally supplied and Rotation and chip_id should be
    //     computed automatically from this! i.e., possibly this should not be
    //     part of StrixelSensorConfig!
    InclusiveROI roi_module;

    // --- Orientation
    // --- Rotation of the chip (Normal / Inverse)
    //     Used to mirror group ROIs and determine mod ordering.
    defs::Rotation rotation;
    std::optional<int> chip_id; // only relevant for multiple sensors on module

  private:
    friend StrixelSensorConfig
    makeSensorConfig(legacy::SensorKey, std::optional<defs::Rotation> user_rot,
                     std::optional<int> chip_id, defs::BondShift);

    // dumb, private constructor! (To decouple responsibilities)
    StrixelSensorConfig(legacy::SensorKey key_, legacy::ChipGeometry chip_geometry_,
                        defs::BondShift bond_shift_,
                        legacy::StrixelGeometry strixel_geometry_,
                        InclusiveROI roi_module_, defs::Rotation rotation_,
                        std::optional<int> chip_id_)
        : key(key_), chip_geometry(chip_geometry_), bond_shift(bond_shift_),
          strixel_geometry(strixel_geometry_), roi_module(roi_module_),
          rotation(rotation_), chip_id(chip_id_) {}
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
defs::Rotation autoRotate(int chip_id);

} // namespace aare::remap::geom

namespace aare::remap::resolve {

legacy::StrixelGeometry const &strixelGeometry(legacy::SensorKey);
legacy::ChipGeometry chipGeometry(legacy::SensorKey);
aare::InclusiveROI moduleROI(legacy::SensorKey, std::optional<int> chip_id);

} // namespace aare::remap::resolve

namespace aare::remap::algo {

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
model::MappingResult generateUnitMap(aare::InclusiveROI const &roi_user,
                                     aare::InclusiveROI const &roi_group,
                                     int multiplicator, defs::Rotation rot,
                                     int shifty = 0);

/**
 * Utility to join to separately mapped core units of a Strixel Quad (bottom and
 * top half with gap-pixels in-between)
 */
model::MappingResult joinQuadMaps(model::MappingResult const &bottom,
                                  model::MappingResult const &top,
                                  int gap_rows);

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
model::MappingResult generateMPStrixelMapping(
    aare::InclusiveROI const &roi_user_module, legacy::SensorKey key, int chip_id,
    std::optional<defs::Rotation> user_rot, defs::BondShift);

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
model::MappingResult generateQuadStrixelMapping(
    aare::InclusiveROI const &roi_user_module, legacy::SensorKey key,
    std::optional<defs::Rotation> user_rot, defs::BondShift);

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
                T const &value = input[flat_index];
                output(row, col) = value;
                // output(row, col) = static_cast<T>(input[flat_index]);
            } else {
                output(row, col) = static_cast<T>(0); // or nan?
            }
        }
    }
}

} // namespace aare::remap::algo

namespace aare::remap::format {

/**
 * Helpers for printing
 */
static inline std::string toString(legacy::SensorKey);
static inline std::string toString(legacy::SensorLayout);
static inline std::string toString(legacy::SensorTech);
static inline std::string toString(legacy::SensorRevision);
static inline std::string toString(defs::Rotation);
static inline std::string toString(model::StrixelSensorConfig const &c);
inline std::ostream &operator<<(std::ostream &os,
                                model::StrixelSensorConfig const &c);

} // namespace aare::remap::format