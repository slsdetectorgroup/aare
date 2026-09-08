#include <pybind11/pybind11.h>

#include "aare/StrixelPixelRemapping/StrixelPixelRemapConfig.hpp"
#include "aare/StrixelPixelRemapping/StrixelPixelRemapGenerate.hpp"

namespace py = pybind11;

void define_predefinedConfigs(py::module &m) {

    // Predefined strixel geometries
    m.attr("StrxP25") = aare::remap::config::jungfrau::StrxP25;
    // Strixel geometry for 25 µm pitch strixels on iLGAD sensors (multiplicity
    // = 3)

    m.attr("StrxP15") = aare::remap::config::jungfrau::StrxP15;
    //"Strixel geometry for 15 µm pitch strixels on iLGAD sensors "
    //"(multiplicity = 5)";
    m.attr("StrxP18") = aare::remap::config::jungfrau::StrxP18;
    //"Strixel geometry for 18 µm pitch strixels on iLGAD sensors "
    //"(multiplicity = 4)";
    m.attr("StrxP37") = aare::remap::config::jungfrau::StrxP37;
    //"Strixel geometry for 37 µm pitch strixels on iLGAD sensors "
    //"(multiplicity = 2)";

    // Predefined sensor placements
    m.attr("Chip1") = aare::remap::config::jungfrau::Chip1;
    // Placement of the 2x2cm iLGAD sensor on the second chip (Chip1) of the
    // Jungfrau module with no rotation applied.

    m.attr("Chip6") = aare::remap::config::jungfrau::Chip6;
    //"Placement of the 2x2cm iLGAD sensor on the seventh chip (Chip6) of "
    //"the Jungfrau module"
    //"with a 180-degree rotation applied.";
    m.attr("Quad") = aare::remap::config::jungfrau::Quad;
    //"Placement of the 4x4cm iLGAD sensor on the quad "
    //"(Chip1+Chip2+Chip5+Chip6) of the Jungfrau module"
    //"with no rotation applied.";

    // Predefined sensor geometries
    m.attr("SingleChipMP_iLGAD_pix") =
        aare::remap::config::jungfrau::SingleChipMP_iLGAD_pix;
    // "Pixel geometry of the 2x2 cm iLGAD sensor";
    m.attr("Quad_iLGAD_pix") = aare::remap::config::jungfrau::Quad_iLGAD_pix;
    // "Pixel geometry of the 4x4 cm iLGAD sensor";
    m.attr("SingleChipMP_TEW_pix") =
        aare::remap::config::jungfrau::SingleChipMP_TEW_pix;
    // "Pixel geometry of the 2x2 cm TEW sensor";

    // Predefined strixel groups
    m.attr("SingleChipMP_iLGAD_P25") =
        aare::remap::config::jungfrau::SingleChipMP_iLGAD_P25;

    // "Strixel group of 25 µm pitch strixels on the 2x2 cm iLGAD sensor";
    m.attr("SingleChipMP_iLGAD_P15") =
        aare::remap::config::jungfrau::SingleChipMP_iLGAD_P15;

    // "Strixel group of 15 µm pitch strixels on the 2x2 cm iLGAD sensor";
    m.attr("SingleChipMP_iLGAD_P18") =
        aare::remap::config::jungfrau::SingleChipMP_iLGAD_P18;

    // "Strixel group of 18.75 µm pitch strixels on the 2x2 cm iLGAD sensor";
    m.attr("SingleChipMP_TEW_P25") =
        aare::remap::config::jungfrau::SingleChipMP_TEW_P25;

    // "Strixel group of 25 µm pitch strixels on the 2x2 cm TEW sensor";
    m.attr("SingleChipMP_TEW_P15") =
        aare::remap::config::jungfrau::SingleChipMP_TEW_P15;
    // "Strixel group of 15 µm pitch strixels on the 2x2 cm TEW sensor";
    m.attr("SingleChipMP_TEW_P18") =
        aare::remap::config::jungfrau::SingleChipMP_TEW_P18;
    // "Strixel group of 18.75 µm pitch strixels on the 2x2 cm TEW sensor";
    m.attr("Quad_iLGAD_bottomhalf") =
        aare::remap::config::jungfrau::Quad_iLGAD_bottomhalf;
    // "Strixel group of 25 µm pitch strixels located on the bottom half of "
    // "4x4 cm iLGAD sensor";
    m.attr("Quad_iLGAD_tophalf") =
        aare::remap::config::jungfrau::Quad_iLGAD_tophalf;
    // "Strixel group of 25 µm pitch strixels located on the top half of 4x4 "
    // "cm iLGAD sensor";

    // Predefined sensor configurations
    m.attr("SingleChipMP_iLGAD") =
        aare::remap::config::jungfrau::SingleChipMP_iLGAD;
    // "Sensor configuration of the 2x2 cm iLGAD sensor with all strixel
    // groups";
    m.attr("SingleChipMP_TEW") =
        aare::remap::config::jungfrau::SingleChipMP_TEW;
    // "Sensor configuration of the 2x2 cm TEW sensor with all strixel groups";
    m.attr("Quad_iLGAD") = aare::remap::config::jungfrau::Quad_iLGAD;
    // "Sensor configuration of the 4x4 cm iLGAD sensor with all strixel "
    // "groups";
}

void define_predefinedStrixelPixelMaps(py::module &m) {
    m.attr("jungfrau_ilgad_singlechip_25um_strixel_map") = py::cpp_function(
        &aare::remap::generate::jungfrau_ilgad_singlechip_25um_strixel_map,
        py::arg("user_roi").noconvert(), py::arg("placement").noconvert(),
        py::arg("bond_shift").noconvert() = aare::remap::defs::BondShift{0, 0},
        R"(
            Generates a strixel-to-pixel remapping map for the Strx25 strixel group on a Jungfrau ILGAD sensor

            Parameters
            ----------
            user_roi : InclusiveROI
                ROI in global module coordinate system.
            placement : SensorModulePlacement
                Placement and orientation of the sensor on the module.
            bond_shift : BondShift, optional
                Bonding shift applied before the configured sensor placement rotation. Default is (0, 0).
            Returns
            -------
            StrixelGroupToPixelMap
                map(row, col) contains the flattened pixel index of the corresponding source pixel in the user-provided input ROI for strixel defined at (row, col).
                An entry of -1 indicates that the corresponding strixel position has no valid source pixel.
        )");

    m.attr("jungfrau_ilgad_singlechip_15um_strixel_map") = py::cpp_function(
        &aare::remap::generate::jungfrau_ilgad_singlechip_15um_strixel_map,
        py::arg("user_roi").noconvert(), py::arg("placement").noconvert(),
        py::arg("bond_shift").noconvert() = aare::remap::defs::BondShift{0, 0},
        R"(
            Generates a strixel-to-pixel remapping map for the Strx15 strixel group on a Jungfrau ILGAD sensor

            Parameters
            ----------
            user_roi : InclusiveROI
                ROI in global module coordinate system.
            placement : SensorModulePlacement
                Placement and orientation of the sensor on the module.
            bond_shift : BondShift, optional
                Bonding shift applied before the configured sensor placement rotation. Default is (0, 0).
            Returns
            -------
            StrixelGroupToPixelMap
                map(row, col) contains the flattened pixel index of the corresponding source pixel in the user-provided input ROI for strixel defined at (row, col).
                An entry of -1 indicates that the corresponding strixel position has no valid source pixel.
        )");

    m.attr("jungfrau_ilgad_singlechip_18um_strixel_map") = py::cpp_function(
        &aare::remap::generate::jungfrau_ilgad_singlechip_18um_strixel_map,
        py::arg("user_roi").noconvert(), py::arg("placement").noconvert(),
        py::arg("bond_shift").noconvert() = aare::remap::defs::BondShift{0, 0},
        R"(
                Generates a strixel-to-pixel remapping map for the Strx18 strixel group on a Jungfrau ILGAD sensor

                Parameters
                ----------
                user_roi : InclusiveROI
                    ROI in global module coordinate system.
                placement : SensorModulePlacement
                    Placement and orientation of the sensor on the module.
                bond_shift : BondShift, optional
                    Bonding shift applied before the configured sensor placement rotation. Default is (0, 0).
                Returns
                -------
                StrixelGroupToPixelMap
                    map(row, col) contains the flattened pixel index of the corresponding source pixel in the user-provided input ROI for strixel defined at (row, col).
                    An entry of -1 indicates that the corresponding strixel position has no valid source pixel.
                )");

    m.attr("jungfrau_ilgad_strixel_maps") = py::cpp_function(
        &aare::remap::generate::jungfrau_ilgad_quad_25um_strixel_map,
        py::arg("user_roi").noconvert(), py::arg("placement").noconvert(),
        py::arg("bond_shift").noconvert() = aare::remap::defs::BondShift{0, 0},
        R"(
                Generates a list of strixel-to-pixel remapping map for each strixel group on a Jungfrau ILGAD quad sensor

                Parameters
                ----------
                user_roi : InclusiveROI
                    ROI in global module coordinate system.
                placement : SensorModulePlacement
                    Placement and orientation of the sensor on the module.
                bond_shift : BondShift, optional
                    Bonding shift applied before the configured sensor placement rotation. Default is (0, 0).
                Returns
                -------
                list[StrixelGroupToPixelMap] 
                    A list of StrixelGroupToPixelMap, one for each strixel group on the sensor.
                    Each map(row, col) contains the flattened pixel index of the corresponding source pixel in the user-provided input ROI for strixel defined at (row, col).
                    An entry of -1 indicates that the corresponding strixel position has no valid source pixel.
                )");

    // TEW sensor strixel maps
    m.attr("jungfrau_tew_singlechip_25um_strixel_map") = py::cpp_function(
        &aare::remap::generate::jungfrau_tew_singlechip_25um_strixel_map,
        py::arg("user_roi").noconvert(), py::arg("placement").noconvert(),
        py::arg("bond_shift").noconvert() = aare::remap::defs::BondShift{0, 0},
        R"(
                Generates a strixel-to-pixel remapping map for the Strx25 strixel group on a Jungfrau TEW sensor

                Parameters
                ----------
                user_roi : InclusiveROI
                    ROI in global module coordinate system.
                placement : SensorModulePlacement
                    Placement and orientation of the sensor on the module.
                bond_shift : BondShift, optional
                    Bonding shift applied before the configured sensor placement rotation. Default is (0, 0).
                Returns
                -------
                StrixelGroupToPixelMap
                    map(row, col) contains the flattened pixel index of the corresponding source pixel in the user-provided input ROI for strixel defined at (row, col).
                    An entry of -1 indicates that the corresponding strixel position has no valid source pixel.
                )");

    m.attr("jungfrau_tew_singlechip_15um_strixel_map") = py::cpp_function(
        &aare::remap::generate::jungfrau_tew_singlechip_15um_strixel_map,
        py::arg("user_roi").noconvert(), py::arg("placement").noconvert(),
        py::arg("bond_shift").noconvert() = aare::remap::defs::BondShift{0, 0},
        R"(
                Generates a strixel-to-pixel remapping map for the Strx15 strixel group on a Jungfrau TEW sensor

                Parameters
                ----------
                user_roi : InclusiveROI
                    ROI in global module coordinate system.
                placement : SensorModulePlacement
                    Placement and orientation of the sensor on the module.
                bond_shift : BondShift, optional
                    Bonding shift applied before the configured sensor placement rotation. Default is (0, 0).
                Returns
                -------
                StrixelGroupToPixelMap
                    map(row, col) contains the flattened pixel index of the corresponding source pixel in the user-provided input ROI for strixel defined at (row, col).
                    An entry of -1 indicates that the corresponding strixel position has no valid source pixel.
                )");

    m.attr("jungfrau_tew_singlechip_18um_strixel_map") = py::cpp_function(
        &aare::remap::generate::jungfrau_tew_singlechip_18um_strixel_map,
        py::arg("user_roi").noconvert(), py::arg("placement").noconvert(),
        py::arg("bond_shift").noconvert() = aare::remap::defs::BondShift{0, 0},
        R"(
                Generates a strixel-to-pixel remapping map for the Strx18 strixel group on a Jungfrau TEW sensor

                Parameters
                ----------
                user_roi : InclusiveROI
                    ROI in global module coordinate system.
                placement : SensorModulePlacement
                    Placement and orientation of the sensor on the module.
                bond_shift : BondShift, optional
                    Bonding shift applied before the configured sensor placement rotation. Default is (0, 0).
                Returns
                -------
                StrixelGroupToPixelMap
                    map(row, col) contains the flattened pixel index of the corresponding source pixel in the user-provided input ROI for strixel defined at (row, col).
                    An entry of -1 indicates that the corresponding strixel position has no valid source pixel.
                )");

    m.attr("jungfrau_tew_strixel_maps") = py::cpp_function(
        &aare::remap::generate::jungfrau_tew_singlechip_multipitch_strixel_maps,
        py::arg("user_roi").noconvert(), py::arg("placement").noconvert(),
        py::arg("bond_shift").noconvert() = aare::remap::defs::BondShift{0, 0},
        R"(
                Generates a list of strixel-to-pixel remapping map for each strixel group on a Jungfrau TEW sensor

                Parameters
                ----------
                user_roi : InclusiveROI
                    ROI in global module coordinate system.
                placement : SensorModulePlacement
                    Placement and orientation of the sensor on the module.
                bond_shift : BondShift, optional
                    Bonding shift applied before the configured sensor placement rotation. Default is (0, 0).
                Returns
                -------
                list[StrixelGroupToPixelMap] 
                    A list of StrixelGroupToPixelMap, one for each strixel group on the sensor.
                    Each map(row, col) contains the flattened pixel index of the corresponding source pixel in the user-provided input ROI for strixel defined at (row, col).
                    An entry of -1 indicates that the corresponding strixel position has no valid source pixel.
                )");

    // Jungfrau quad sensor strixel maps
    m.attr("jungfrau_ilgad_quadbottom_25um_strixel_map") = py::cpp_function(
        &aare::remap::generate::jungfrau_ilgad_quadbottom_25um_strixel_map,
        py::arg("user_roi").noconvert(), py::arg("placement").noconvert(),
        py::arg("bond_shift").noconvert() = aare::remap::defs::BondShift{0, 0},
        R"(
                Generates a strixel-to-pixel remapping map for the bottom half of the Strx25 strixel group on a Jungfrau ILGAD quad sensor

                Parameters
                ----------
                user_roi : InclusiveROI
                    ROI in global module coordinate system.
                placement : SensorModulePlacement
                    Placement and orientation of the sensor on the module.
                bond_shift : BondShift, optional
                    Bonding shift applied before the configured sensor placement rotation. Default is (0, 0).
                Returns
                -------
                StrixelGroupToPixelMap
                    map(row, col) contains the flattened pixel index of the corresponding source pixel in the user-provided input ROI for strixel defined at (row, col).
                    An entry of -1 indicates that the corresponding strixel position has no valid source pixel.
                )");

    m.attr("jungfrau_ilgad_quadtop_25um_strixel_map") = py::cpp_function(
        &aare::remap::generate::jungfrau_ilgad_quadtop_25um_strixel_map,
        py::arg("user_roi").noconvert(), py::arg("placement").noconvert(),
        py::arg("bond_shift").noconvert() = aare::remap::defs::BondShift{0, 0},
        R"(
                Generates a strixel-to-pixel remapping map for the top half of the Strx25 strixel group on a Jungfrau ILGAD quad sensor

                Parameters
                ----------
                user_roi : InclusiveROI
                    ROI in global module coordinate system.
                placement : SensorModulePlacement
                    Placement and orientation of the sensor on the module.
                bond_shift : BondShift, optional
                    Bonding shift applied before the configured sensor placement rotation. Default is (0, 0).
                Returns
                -------
                StrixelGroupToPixelMap
                    map(row, col) contains the flattened pixel index of the corresponding source pixel in the user-provided input ROI for strixel defined at (row, col).
                    An entry of -1 indicates that the corresponding strixel position has no valid source pixel.
                )");

    m.attr("jungfrau_ilgad_quad_25um_strixel_map") = py::cpp_function(
        &aare::remap::generate::jungfrau_ilgad_quad_25um_strixel_map,
        py::arg("user_roi").noconvert(), py::arg("placement").noconvert(),
        py::arg("bond_shift").noconvert() = aare::remap::defs::BondShift{0, 0},
        R"(
                Generates a strixel-to-pixel remapping map for the entire Strx25 strixel group on a Jungfrau ILGAD quad sensor

                Parameters
                ----------
                user_roi : InclusiveROI
                    ROI in global module coordinate system.
                placement : SensorModulePlacement
                    Placement and orientation of the sensor on the module.
                bond_shift : BondShift, optional
                    Bonding shift applied before the configured sensor placement rotation. Default is (0, 0).
                Returns
                -------
                StrixelGroupToPixelMap
                    combined maps of the bottom and top halves of the Strx25 strixel group on a Jungfrau ILGAD quad sensor.
                    map(row, col) contains the flattened pixel index of the corresponding source pixel in the user-provided input ROI for strixel defined at (row, col).
                    An entry of -1 indicates that the corresponding strixel position has no valid source pixel.
                )");
}