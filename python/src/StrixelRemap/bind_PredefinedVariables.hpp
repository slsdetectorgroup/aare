#include <pybind11/pybind11.h>

#include "aare/StrixelPixelRemapping/StrixelPixelMaps.hpp"
#include "aare/StrixelPixelRemapping/StrixelPixelRemapConfig.hpp"

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

    py::class_<aare::remap::Jungfrau_iLGAD_StrixelPixelMap,
               aare::remap::StrixelPixelMap<3, 3>>(
        m, "Jungfrau_iLGAD_StrixelPixelMap")
        .def(py::init<const aare::remap::defs::SensorModulePlacement &,
                      const aare::remap::defs::BondShift &>(),
             py::arg("module_placement").noconvert(),
             py::arg("bond_shift").noconvert() =
                 aare::remap::defs::BondShift{0, 0},
             R"(
                Construct a new Jungfrau_iLGAD_StrixelPixelMap object.

                Parameters
                ----------
                module_placement : SensorModulePlacement
                    Placement and orientation of the sensor on the module.
                bond_shift : BondShift, optional
                    Bonding shift applied before the configured sensor rotation.
                    Default is (0, 0).
            )");

    py::class_<aare::remap::Jungfrau_TEW_StrixelPixelMap,
               aare::remap::StrixelPixelMap<3, 3>>(
        m, "Jungfrau_TEW_StrixelPixelMap")
        .def(py::init<const aare::remap::defs::SensorModulePlacement &,
                      const aare::remap::defs::BondShift &>(),
             py::arg("module_placement").noconvert(),
             py::arg("bond_shift").noconvert() =
                 aare::remap::defs::BondShift{0, 0},
             R"(
                Construct a new Jungfrau_TEW_StrixelPixelMap object.

                Parameters
                ----------
                module_placement : SensorModulePlacement
                    Placement and orientation of the sensor on the module.
                bond_shift : BondShift, optional
                    Bonding shift applied before the configured sensor rotation.
                    Default is (0, 0).
            )");

    py::class_<aare::remap::Jungfrau_iLGAD_Quad_StrixelPixelMap,
               aare::remap::StrixelPixelMap<2, 1>>(
        m, "Jungfrau_iLGAD_Quad_StrixelPixelMap")
        .def(py::init<const aare::remap::defs::BondShift &>(),
             py::arg("bond_shift").noconvert() =
                 aare::remap::defs::BondShift{0, 0},
             R"(
                Constructor for Jungfrau_iLGAD_Quad_StrixelPixelMap object.

                Parameters
                ----------
                bond_shift : BondShift, optional
                    Bonding shift applied before the configured sensor rotation.
                    Default is (0, 0).
            )");
}
