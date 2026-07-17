#pragma once

#include "aare/RemapAlgorithm.hpp"
#include "aare/RemapConfig.hpp"

namespace aare::remap::generate {

/************************************
 * Single chip, multi-pitch, iLGAD
 ************************************/
inline defs::StrixelGroupToPixelMap
jungfrau_ilgad_singlechip_25um_strixel_map(InclusiveROI rx_roi,
                                           defs::SensorModulePlacement placement,
                                           defs::BondShift bs = {0, 0}) {
    std::cout << " === JUNGFRAU iLGAD SINGLE-CHIP 25um PITCH === \n";
    return algo::strixel_to_pixel_map(config::jungfrau::SingleChipMP_iLGAD_P25,
                                      config::jungfrau::SingleChipMP_iLGAD_pix,
                                      placement, rx_roi, bs);
}

inline defs::StrixelGroupToPixelMap
jungfrau_ilgad_singlechip_15um_strixel_map(InclusiveROI rx_roi,
                                           defs::SensorModulePlacement placement,
                                           defs::BondShift bs = {0, 0}) {
    std::cout << " === JUNGFRAU iLGAD SINGLE-CHIP 15um PITCH === \n";
    return algo::strixel_to_pixel_map(config::jungfrau::SingleChipMP_iLGAD_P15,
                                      config::jungfrau::SingleChipMP_iLGAD_pix,
                                      placement, rx_roi, bs);
};

inline defs::StrixelGroupToPixelMap
jungfrau_ilgad_singlechip_18um_strixel_map(InclusiveROI rx_roi,
                                           defs::SensorModulePlacement placement,
                                           defs::BondShift bs = {0, 0}) {
    std::cout << " === JUNGFRAU iLGAD SINGLE-CHIP 18.75um PITCH === \n";
    return algo::strixel_to_pixel_map(config::jungfrau::SingleChipMP_iLGAD_P18,
                                      config::jungfrau::SingleChipMP_iLGAD_pix,
                                      placement, rx_roi, bs);
};

// Probably, one could get rid of this
inline std::vector<defs::StrixelGroupToPixelMap>
jungfrau_ilgad_singlechip_multipitch_strixel_maps(InclusiveROI rx_roi,
                                                  int chip_id = 1,
                                                  defs::BondShift bs = {0, 0}) {
    defs::SensorModulePlacement placement;
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
    defs::SensorConfig configs{config::jungfrau::SingleChipMP_iLGAD_pix,
                               {config::jungfrau::SingleChipMP_iLGAD_P25,
                                config::jungfrau::SingleChipMP_iLGAD_P15,
                                config::jungfrau::SingleChipMP_iLGAD_P18}};

    return algo::strixel_to_pixel_maps(configs, placement, rx_roi, bs);
};

// More generic overload
inline std::vector<defs::StrixelGroupToPixelMap>
jungfrau_ilgad_singlechip_multipitch_strixel_maps(
    InclusiveROI rx_roi, defs::SensorModulePlacement placement,
    defs::BondShift bs = {0, 0}) {
    defs::SensorConfig configs{config::jungfrau::SingleChipMP_iLGAD_pix,
                               {config::jungfrau::SingleChipMP_iLGAD_P25,
                                config::jungfrau::SingleChipMP_iLGAD_P15,
                                config::jungfrau::SingleChipMP_iLGAD_P18}};

    return algo::strixel_to_pixel_maps(configs, placement, rx_roi, bs);
};

/************************************
 * Single chip, multi-pitch, TEW
 ************************************/
inline defs::StrixelGroupToPixelMap
jungfrau_tew_singlechip_25um_strixel_map(InclusiveROI rx_roi,
                                         defs::SensorModulePlacement placement,
                                         defs::BondShift bs = {0, 0}) {
    std::cout << " === JUNGFRAU TEW SINGLE-CHIP 25um PITCH === \n";
    return algo::strixel_to_pixel_map(config::jungfrau::SingleChipMP_TEW_P25,
                                      config::jungfrau::SingleChipMP_TEW_pix,
                                      placement, rx_roi, bs);
}

inline defs::StrixelGroupToPixelMap
jungfrau_tew_singlechip_15um_strixel_map(InclusiveROI rx_roi,
                                         defs::SensorModulePlacement placement,
                                         defs::BondShift bs = {0, 0}) {
    std::cout << " === JUNGFRAU TEW SINGLE-CHIP 15um PITCH === \n";
    return algo::strixel_to_pixel_map(config::jungfrau::SingleChipMP_TEW_P15,
                                      config::jungfrau::SingleChipMP_TEW_pix,
                                      placement, rx_roi, bs);
};

inline defs::StrixelGroupToPixelMap
jungfrau_tew_singlechip_18um_strixel_map(InclusiveROI rx_roi,
                                         defs::SensorModulePlacement placement,
                                         defs::BondShift bs = {0, 0}) {
    std::cout << " === JUNGFRAU TEW SINGLE-CHIP 18.75um PITCH === \n";
    return algo::strixel_to_pixel_map(config::jungfrau::SingleChipMP_TEW_P18,
                                      config::jungfrau::SingleChipMP_TEW_pix,
                                      placement, rx_roi, bs);
};

// Get rid?
inline std::vector<defs::StrixelGroupToPixelMap>
jungfrau_tew_singlechip_multipitch_strixel_maps(InclusiveROI rx_roi,
                                                int chip_id = 1,
                                                defs::BondShift bs = {0, 0}) {
    defs::SensorModulePlacement placement;
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
    defs::SensorConfig configs{config::jungfrau::SingleChipMP_TEW_pix,
                               {config::jungfrau::SingleChipMP_TEW_P25,
                                config::jungfrau::SingleChipMP_TEW_P15,
                                config::jungfrau::SingleChipMP_TEW_P18}};

    return algo::strixel_to_pixel_maps(configs, placement, rx_roi, bs);
};

// More generic overload
inline std::vector<defs::StrixelGroupToPixelMap>
jungfrau_tew_singlechip_multipitch_strixel_maps(InclusiveROI rx_roi,
                                                defs::SensorModulePlacement placement,
                                                defs::BondShift bs = {0, 0}) {
    defs::SensorConfig configs{config::jungfrau::SingleChipMP_TEW_pix,
                               {config::jungfrau::SingleChipMP_TEW_P25,
                                config::jungfrau::SingleChipMP_TEW_P15,
                                config::jungfrau::SingleChipMP_TEW_P18}};

    return algo::strixel_to_pixel_maps(configs, placement, rx_roi, bs);
};

/************************************
 * Quad, 25 um, iLGAD
 ************************************/
inline defs::StrixelGroupToPixelMap
jungfrau_ilgad_quadbottom_25um_strixel_map(InclusiveROI rx_roi,
                                           defs::SensorModulePlacement placement) {
    return algo::strixel_to_pixel_map(config::jungfrau::Quad_iLGAD_bottomhalf,
                                      config::jungfrau::Quad_iLGAD_pix,
                                      placement, rx_roi);
}

inline defs::StrixelGroupToPixelMap
jungfrau_ilgad_quadtop_25um_strixel_map(InclusiveROI rx_roi,
                                        defs::SensorModulePlacement placement) {
    return algo::strixel_to_pixel_map(config::jungfrau::Quad_iLGAD_tophalf,
                                      config::jungfrau::Quad_iLGAD_pix,
                                      placement, rx_roi);
}

inline std::vector<defs::StrixelGroupToPixelMap>
jungfrau_ilgad_quad_25um_strixel_maps(InclusiveROI rx_roi,
                                      defs::SensorModulePlacement placement) {

    defs::SensorConfig configs{config::jungfrau::Quad_iLGAD_pix,
                               {config::jungfrau::Quad_iLGAD_bottomhalf,
                                config::jungfrau::Quad_iLGAD_tophalf}};

    return algo::strixel_to_pixel_maps(configs, placement, rx_roi);
}

inline defs::StrixelGroupToPixelMap
jungfrau_ilgad_quad_25um_strixel_map(InclusiveROI rx_roi,
                                     defs::SensorModulePlacement placement,
                                     defs::BondShift bs = {0, 0}) {
    std::vector<int> gap_rows{12, 0};
    auto maps = jungfrau_ilgad_quad_25um_strixel_maps(rx_roi, placement);
    return algo::combine_maps(maps, gap_rows);
}
} // namespace aare::remap::generate