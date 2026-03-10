#pragma once

#include "aare/RemapAlgorithm.hpp"
#include "aare/RemapConfig.hpp"

namespace aare::remap::generate {

inline defs::StrixelGroupToPixelMap
jungfrau_ilgad_singlechip_25um_strixel_map(InclusiveROI rx_roi, int chip_id = 1,
                                           defs::BondShift bs = {0, 0}) {
    if (chip_id == 1)
        return algo::strixel_to_pixel_map(
            config::jungfrau::SingleChipMP_iLGAD_P25, config::jungfrau::Chip1,
            rx_roi, bs);
    else if (chip_id == 6)
        return algo::strixel_to_pixel_map(
            config::jungfrau::SingleChipMP_iLGAD_P25, config::jungfrau::Chip6,
            rx_roi, bs);
    else {
        // or allow user-defined sensor placement
        // (that would mean something like std::optional<int> chip_id and
        // std::optional<InclusiveROI> sensor_placement)
        throw std::runtime_error("Invalid sensor placement.");
    }
}

inline defs::StrixelGroupToPixelMap
jungfrau_ilgad_singlechip_15um_strixel_map(InclusiveROI rx_roi, int chip_id = 1,
                                           defs::BondShift bs = {0, 0}) {
    if (chip_id == 1)
        return algo::strixel_to_pixel_map(
            config::jungfrau::SingleChipMP_iLGAD_P15, config::jungfrau::Chip1,
            rx_roi, bs);
    else if (chip_id == 6)
        return algo::strixel_to_pixel_map(
            config::jungfrau::SingleChipMP_iLGAD_P15, config::jungfrau::Chip6,
            rx_roi, bs);
    else {
        // or allow user-defined sensor placement
        // (that would mean something like std::optional<int> chip_id and
        // std::optional<InclusiveROI> sensor_placement)
        throw std::runtime_error("Invalid sensor placement.");
    }
};

inline defs::StrixelGroupToPixelMap
jungfrau_ilgad_singlechip_18um_strixel_map(InclusiveROI rx_roi, int chip_id = 1,
                                           defs::BondShift bs = {0, 0}) {
    if (chip_id == 1)
        return algo::strixel_to_pixel_map(
            config::jungfrau::SingleChipMP_iLGAD_P18, config::jungfrau::Chip1,
            rx_roi, bs);
    else if (chip_id == 6)
        return algo::strixel_to_pixel_map(
            config::jungfrau::SingleChipMP_iLGAD_P18, config::jungfrau::Chip6,
            rx_roi, bs);
    else {
        // or allow user-defined sensor placement
        // (that would mean something like std::optional<int> chip_id and
        // std::optional<InclusiveROI> sensor_placement)
        throw std::runtime_error("Invalid sensor placement.");
    }
};

inline std::vector<defs::StrixelGroupToPixelMap>
jungfrau_ilgad_singlechip_multipitch_strixel_maps(InclusiveROI rx_roi,
                                                  int chip_id = 1,
                                                  defs::BondShift bs = {0, 0}) {
    defs::SensorPlacement placement;
    if (chip_id == 1)
        placement = config::jungfrau::Chip1;
    else if (chip_id == 6)
        placement = config::jungfrau::Chip6;
    else {
        // or allow user-defined sensor placement
        // (that would mean something like std::optional<int> chip_id and
        // std::optional<InclusiveROI> sensor_placement)
        throw std::runtime_error("Invalid sensor placement.");
    }
    defs::SensorConfig configs{{config::jungfrau::SingleChipMP_iLGAD_P25,
                                config::jungfrau::SingleChipMP_iLGAD_P15,
                                config::jungfrau::SingleChipMP_iLGAD_P18}};
    return algo::strixel_to_pixel_maps(configs, placement, rx_roi, bs);
};
} // namespace aare::remap::generate