// SPDX-License-Identifier: MPL-2.0
#include "aare/File.hpp"
#include "aare/RawFile.hpp"
#include "aare/RawMasterFile.hpp" //needed for ROI

#include <catch2/catch_test_macros.hpp>
#include <filesystem>

#include "aare/DetectorGeometry.hpp"
#include "aare/ROIGeometry.hpp"
#include "test_config.hpp"

TEST_CASE("Simple ROIs on one module", "[DetectorGeometry]") {
    aare::DetectorGeometry geo(aare::xy{1, 1}, 1024, 512);

    REQUIRE(geo.get_module_geometries(0).origin_x == 0);
    REQUIRE(geo.get_module_geometries(0).origin_y == 0);
    REQUIRE(geo.get_module_geometries(0).width == 1024);
    REQUIRE(geo.get_module_geometries(0).height == 512);
    REQUIRE(geo.modules_x() == 1);
    REQUIRE(geo.modules_y() == 1);
    REQUIRE(geo.pixels_x() == 1024);
    REQUIRE(geo.pixels_y() == 512);

    SECTION("ROI is the whole module") {
        aare::ROI roi;
        roi.xmin = 0;
        roi.xmax = 1024;
        roi.ymin = 0;
        roi.ymax = 512;

        aare::ROIGeometry roi_geometry(roi, geo);

        REQUIRE(roi_geometry.pixels_x() == 1024);
        REQUIRE(roi_geometry.pixels_y() == 512);
        REQUIRE(roi_geometry.num_modules_in_roi() == 1);
        auto module_geometry =
            geo.get_module_geometries(roi_geometry.module_indices_in_roi(0));
        REQUIRE(module_geometry.height == 512);
        REQUIRE(module_geometry.width == 1024);
        REQUIRE(module_geometry.origin_x == 0);
        REQUIRE(module_geometry.origin_y == 0);
    }
    SECTION("ROI is the top left corner of the module") {
        aare::ROI roi;
        roi.xmin = 100;
        roi.xmax = 200;
        roi.ymin = 150;
        roi.ymax = 200;

        aare::ROIGeometry roi_geometry(roi, geo);

        REQUIRE(roi_geometry.pixels_x() == 100);
        REQUIRE(roi_geometry.pixels_y() == 50);
        REQUIRE(roi_geometry.num_modules_in_roi() == 1);
        auto module_geometry =
            geo.get_module_geometries(roi_geometry.module_indices_in_roi(0));
        REQUIRE(module_geometry.height == 50);
        REQUIRE(module_geometry.width == 100);
        REQUIRE(module_geometry.origin_x == 0);
        REQUIRE(module_geometry.origin_y == 0);
    }
}

TEST_CASE("Two modules side by side", "[DetectorGeometry]") {
    aare::DetectorGeometry geo(aare::xy{1, 2}, 1024, 512);

    REQUIRE(geo.get_module_geometries(0).origin_x == 0);
    REQUIRE(geo.get_module_geometries(0).origin_y == 0);
    REQUIRE(geo.get_module_geometries(0).width == 1024);
    REQUIRE(geo.get_module_geometries(0).height == 512);
    REQUIRE(geo.get_module_geometries(1).origin_x == 1024);
    REQUIRE(geo.get_module_geometries(1).origin_y == 0);
    REQUIRE(geo.modules_x() == 2);
    REQUIRE(geo.modules_y() == 1);
    REQUIRE(geo.pixels_x() == 2048);
    REQUIRE(geo.pixels_y() == 512);

    SECTION("ROI is the whole image") {
        aare::ROI roi;
        roi.xmin = 0;
        roi.xmax = 2048;
        roi.ymin = 0;
        roi.ymax = 512;

        aare::ROIGeometry roi_geometry(roi, geo);

        REQUIRE(roi_geometry.pixels_x() == 2048);
        REQUIRE(roi_geometry.pixels_y() == 512);
        REQUIRE(roi_geometry.num_modules_in_roi() == 2);

        auto module_geometry_0 = geo.get_module_geometries(0);
        auto module_geometry_1 = geo.get_module_geometries(1);

        REQUIRE(module_geometry_0.height == 512);
        REQUIRE(module_geometry_0.width == 1024);
        REQUIRE(module_geometry_1.height == 512);
        REQUIRE(module_geometry_1.width == 1024);
        REQUIRE(module_geometry_0.origin_x == 0);
        REQUIRE(module_geometry_1.origin_x == 1024);
        REQUIRE(module_geometry_0.origin_y == 0);
        REQUIRE(module_geometry_1.origin_y == 0);
    }
    SECTION("rectangle on both modules") {
        aare::ROI roi;
        roi.xmin = 800;
        roi.xmax = 1300;
        roi.ymin = 200;
        roi.ymax = 499;

        aare::ROIGeometry roi_geometry(roi, geo);

        REQUIRE(roi_geometry.pixels_x() == 500);
        REQUIRE(roi_geometry.pixels_y() == 299);
        REQUIRE(roi_geometry.num_modules_in_roi() == 2);

        auto module_geometry_0 = geo.get_module_geometries(0);
        auto module_geometry_1 = geo.get_module_geometries(1);

        REQUIRE(module_geometry_0.height == 299);
        REQUIRE(module_geometry_0.width == 224);
        REQUIRE(module_geometry_1.height == 299);
        REQUIRE(module_geometry_1.width == 276);
        REQUIRE(module_geometry_0.origin_x == 0);
        REQUIRE(module_geometry_1.origin_x == 224);
        REQUIRE(module_geometry_0.origin_y == 0);
        REQUIRE(module_geometry_1.origin_y == 0);
    }
}

TEST_CASE("Three modules side by side", "[DetectorGeometry]") {

    aare::DetectorGeometry geo(aare::xy{1, 3}, 1024, 512);

    REQUIRE(geo.get_module_geometries(0).origin_x == 0);
    REQUIRE(geo.get_module_geometries(0).origin_y == 0);
    REQUIRE(geo.get_module_geometries(0).width == 1024);
    REQUIRE(geo.get_module_geometries(0).height == 512);
    REQUIRE(geo.get_module_geometries(1).origin_x == 1024);
    REQUIRE(geo.get_module_geometries(2).origin_x == 2048);
    REQUIRE(geo.modules_x() == 3);
    REQUIRE(geo.modules_y() == 1);
    REQUIRE(geo.pixels_x() == 3072);
    REQUIRE(geo.pixels_y() == 512);

    SECTION("ROI overlapping all three modules") {
        aare::ROI roi;
        roi.xmin = 500;
        roi.xmax = 2500;
        roi.ymin = 0;
        roi.ymax = 123;

        aare::ROIGeometry roi_geometry(roi, geo);
        REQUIRE(roi_geometry.pixels_x() == 2000);
        REQUIRE(roi_geometry.pixels_y() == 123);
        REQUIRE(roi_geometry.num_modules_in_roi() == 3);

        auto module_geometry_0 = geo.get_module_geometries(0);
        auto module_geometry_1 = geo.get_module_geometries(1);
        auto module_geometry_2 = geo.get_module_geometries(2);

        REQUIRE(module_geometry_0.height == 123);
        REQUIRE(module_geometry_0.width == 524);
        REQUIRE(module_geometry_0.origin_x == 0);
        REQUIRE(module_geometry_0.origin_y == 0);

        REQUIRE(module_geometry_1.height == 123);
        REQUIRE(module_geometry_1.width == 1024);
        REQUIRE(module_geometry_1.origin_x == 524);
        REQUIRE(module_geometry_1.origin_y == 0);

        REQUIRE(module_geometry_2.height == 123);
        REQUIRE(module_geometry_2.width == 452);
        REQUIRE(module_geometry_2.origin_x == 1548);
        REQUIRE(module_geometry_2.origin_y == 0);
    }

    SECTION("ROI overlapping last two modules") {
        aare::ROI roi;
        roi.xmin = 1050;
        roi.xmax = 2500;
        roi.ymin = 0;
        roi.ymax = 123;

        aare::ROIGeometry roi_geometry(roi, geo);

        REQUIRE(roi_geometry.pixels_x() == 1450);
        REQUIRE(roi_geometry.pixels_y() == 123);
        REQUIRE(roi_geometry.num_modules_in_roi() == 2);

        auto module_geometry_0 = geo.get_module_geometries(0);
        auto module_geometry_1 = geo.get_module_geometries(1);
        auto module_geometry_2 = geo.get_module_geometries(2);

        REQUIRE(module_geometry_0.height == 512);
        REQUIRE(module_geometry_0.width == 1024);
        REQUIRE(module_geometry_0.origin_x == 0);
        REQUIRE(module_geometry_0.origin_y == 0);

        REQUIRE(module_geometry_1.height == 123);
        REQUIRE(module_geometry_1.width == 998);
        REQUIRE(module_geometry_1.origin_x == 0);
        REQUIRE(module_geometry_1.origin_y == 0);

        REQUIRE(module_geometry_2.height == 123);
        REQUIRE(module_geometry_2.width == 452);
        REQUIRE(module_geometry_2.origin_x == 998);
        REQUIRE(module_geometry_2.origin_y == 0);
    }
}

TEST_CASE("Four modules as a square", "[DetectorGeometry]") {
    aare::DetectorGeometry geo(aare::xy{2, 2}, 1024, 512, aare::xy{1, 2});
    aare::ROI roi;
    roi.xmin = 500;
    roi.xmax = 2000;
    roi.ymin = 500;
    roi.ymax = 600;

    REQUIRE(geo.get_module_geometries(0).origin_x == 0);
    REQUIRE(geo.get_module_geometries(0).origin_y == 0);
    REQUIRE(geo.get_module_geometries(0).width == 1024);
    REQUIRE(geo.get_module_geometries(0).height == 512);
    REQUIRE(geo.get_module_geometries(1).origin_x == 1024);
    REQUIRE(geo.get_module_geometries(1).origin_y == 0);
    REQUIRE(geo.get_module_geometries(2).origin_x == 0);
    REQUIRE(geo.get_module_geometries(2).origin_y == 512);
    REQUIRE(geo.get_module_geometries(3).origin_x == 1024);
    REQUIRE(geo.get_module_geometries(3).origin_y == 512);
    REQUIRE(geo.modules_x() == 2);
    REQUIRE(geo.modules_y() == 2);
    REQUIRE(geo.pixels_x() == 2048);
    REQUIRE(geo.pixels_y() == 1024);

    aare::ROIGeometry roi_geometry(roi, geo);

    REQUIRE(roi_geometry.pixels_x() == 1500);
    REQUIRE(roi_geometry.pixels_y() == 100);
    REQUIRE(roi_geometry.num_modules_in_roi() == 4);

    REQUIRE(geo.get_module_geometries(0).height == 12);
    REQUIRE(geo.get_module_geometries(0).width == 524);
    REQUIRE(geo.get_module_geometries(0).origin_x == 0);
    REQUIRE(geo.get_module_geometries(0).origin_y == 0);

    REQUIRE(geo.get_module_geometries(1).height == 12);
    REQUIRE(geo.get_module_geometries(1).width == 976);
    REQUIRE(geo.get_module_geometries(1).origin_x == 524);
    REQUIRE(geo.get_module_geometries(1).origin_y == 0);

    REQUIRE(geo.get_module_geometries(2).height == 88);
    REQUIRE(geo.get_module_geometries(2).width == 524);
    REQUIRE(geo.get_module_geometries(2).origin_x == 0);
    REQUIRE(geo.get_module_geometries(2).origin_y == 12);

    REQUIRE(geo.get_module_geometries(3).height == 88);
    REQUIRE(geo.get_module_geometries(3).width == 976);
    REQUIRE(geo.get_module_geometries(3).origin_x == 524);
    REQUIRE(geo.get_module_geometries(3).origin_y == 12);
}