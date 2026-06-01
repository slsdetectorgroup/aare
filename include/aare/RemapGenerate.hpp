#pragma once

#include "aare/RemapAlgorithm.hpp"
#include "aare/RemapConfig.hpp"

namespace aare::remap::generate {

/************************************
 * Single chip, multi-pitch, iLGAD
 ************************************/
inline defs::StrixelGroupToPixelMap
jungfrau_ilgad_singlechip_25um_strixel_map(InclusiveROI rx_roi,
                                           defs::SensorPlacement placement,
                                           defs::BondShift bs = {0, 0}) {
    return algo::strixel_to_pixel_map(config::jungfrau::SingleChipMP_iLGAD_P25,
                                      placement, rx_roi, bs);
}

inline defs::StrixelGroupToPixelMap
jungfrau_ilgad_singlechip_15um_strixel_map(InclusiveROI rx_roi,
                                           defs::SensorPlacement placement,
                                           defs::BondShift bs = {0, 0}) {
    return algo::strixel_to_pixel_map(config::jungfrau::SingleChipMP_iLGAD_P15,
                                      placement, rx_roi, bs);
};

inline defs::StrixelGroupToPixelMap
jungfrau_ilgad_singlechip_18um_strixel_map(InclusiveROI rx_roi,
                                           defs::SensorPlacement placement,
                                           defs::BondShift bs = {0, 0}) {
    return algo::strixel_to_pixel_map(config::jungfrau::SingleChipMP_iLGAD_P18,
                                      placement, rx_roi, bs);
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

    std::vector<defs::SensorPlacement> placements(configs.group_configs.size(),
                                                  placement);

    return algo::strixel_to_pixel_maps(configs, placements, rx_roi, bs);
};

/************************************
 * Single chip, multi-pitch, TEW
 ************************************/
inline defs::StrixelGroupToPixelMap
jungfrau_tew_singlechip_25um_strixel_map(InclusiveROI rx_roi,
                                         defs::SensorPlacement placement,
                                         defs::BondShift bs = {0, 0}) {
    return algo::strixel_to_pixel_map(config::jungfrau::SingleChipMP_TEW_P25,
                                      placement, rx_roi, bs);
}

inline defs::StrixelGroupToPixelMap
jungfrau_tew_singlechip_15um_strixel_map(InclusiveROI rx_roi,
                                         defs::SensorPlacement placement,
                                         defs::BondShift bs = {0, 0}) {
    return algo::strixel_to_pixel_map(config::jungfrau::SingleChipMP_TEW_P15,
                                      placement, rx_roi, bs);
};

inline defs::StrixelGroupToPixelMap
jungfrau_tew_singlechip_18um_strixel_map(InclusiveROI rx_roi,
                                         defs::SensorPlacement placement,
                                         defs::BondShift bs = {0, 0}) {
    return algo::strixel_to_pixel_map(config::jungfrau::SingleChipMP_TEW_P18,
                                      placement, rx_roi, bs);
};

inline std::vector<defs::StrixelGroupToPixelMap>
jungfrau_tew_singlechip_multipitch_strixel_maps(InclusiveROI rx_roi,
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
    defs::SensorConfig configs{{config::jungfrau::SingleChipMP_TEW_P25,
                                config::jungfrau::SingleChipMP_TEW_P15,
                                config::jungfrau::SingleChipMP_TEW_P18}};

    std::vector<defs::SensorPlacement> placements(configs.group_configs.size(),
                                                  placement);

    return algo::strixel_to_pixel_maps(configs, placements, rx_roi, bs);
};

/************************************
 * Quad, 25 um, iLGAD
 ************************************/
inline defs::StrixelGroupToPixelMap
jungfrau_ilgad_quadhalf_25um_strixel_map(InclusiveROI rx_roi,
                                         defs::SensorPlacement placement) {
    return algo::strixel_to_pixel_map(config::jungfrau::Quad_iLGAD_half,
                                      placement, rx_roi);
}

inline std::vector<defs::StrixelGroupToPixelMap>
jungfrau_ilgad_quad_25um_strixel_maps(InclusiveROI rx_roi,
                                      defs::SensorPlacement placement) {

    defs::SensorConfig configs{
        {config::jungfrau::Quad_iLGAD_half, config::jungfrau::Quad_iLGAD_half}};

    std::vector<defs::SensorPlacement> placements(configs.group_configs.size(),
                                                  placement);

    placements[1].rotation = algo::flip(placement.rotation);

    return algo::strixel_to_pixel_maps(configs, placements, rx_roi);
}

inline defs::StrixelGroupToPixelMap
jungfrau_ilgad_quad_25um_strixel_map(InclusiveROI rx_roi,
                                     defs::SensorPlacement placement,
                                     defs::BondShift bs = {0, 0}) {
    std::vector<int> gap_rows{12, 0};
    auto maps = jungfrau_ilgad_quad_25um_strixel_maps(rx_roi, placement);
    return algo::combine_maps(maps, gap_rows);
}
} // namespace aare::remap::generate