#include "aare/File.hpp"
#include "aare/RawFile.hpp"
#include "aare/RawMasterFile.hpp" //needed for ROI

#include <catch2/catch_test_macros.hpp>
#include <filesystem>

#include "aare/DetectorGeometry.hpp"
#include "test_config.hpp"

TEST_CASE("Simple ROIs on one module") {
    // DetectorGeometry update_geometry_with_roi(DetectorGeometry geo, aare::ROI
    // roi)
    aare::DetectorGeometry geo(aare::xy{1, 1}, 1024, 512);

    REQUIRE(geo.get_module_geometries(0).origin_x == 0);
    REQUIRE(geo.get_module_geometries(0).origin_y == 0);
    REQUIRE(geo.get_module_geometries(0).width == 1024);
    REQUIRE(geo.get_module_geometries(0).height == 512);

    SECTION("ROI is the whole module") {
        aare::ROI roi;
        roi.xmin = 0;
        roi.xmax = 1024;
        roi.ymin = 0;
        roi.ymax = 512;
        geo.update_geometry_with_roi(roi);

        REQUIRE(geo.pixels_x() == 1024);
        REQUIRE(geo.pixels_y() == 512);
        REQUIRE(geo.modules_x() == 1);
        REQUIRE(geo.modules_y() == 1);
        REQUIRE(geo.get_module_geometries(0).height == 512);
        REQUIRE(geo.get_module_geometries(0).width == 1024);
    }
    SECTION("ROI is the top left corner of the module") {
        aare::ROI roi;
        roi.xmin = 100;
        roi.xmax = 200;
        roi.ymin = 150;
        roi.ymax = 200;
        geo.update_geometry_with_roi(roi);

        REQUIRE(geo.pixels_x() == 100);
        REQUIRE(geo.pixels_y() == 50);
        REQUIRE(geo.modules_x() == 1);
        REQUIRE(geo.modules_y() == 1);
        REQUIRE(geo.get_module_geometries(0).height == 50);
        REQUIRE(geo.get_module_geometries(0).width == 100);
    }

    SECTION("ROI is a small square") {
        aare::ROI roi;
        roi.xmin = 1000;
        roi.xmax = 1010;
        roi.ymin = 500;
        roi.ymax = 510;
        geo.update_geometry_with_roi(roi);

        REQUIRE(geo.pixels_x() == 10);
        REQUIRE(geo.pixels_y() == 10);
        REQUIRE(geo.modules_x() == 1);
        REQUIRE(geo.modules_y() == 1);
        REQUIRE(geo.get_module_geometries(0).height == 10);
        REQUIRE(geo.get_module_geometries(0).width == 10);
    }
    SECTION("ROI is a few columns") {
        aare::ROI roi;
        roi.xmin = 750;
        roi.xmax = 800;
        roi.ymin = 0;
        roi.ymax = 512;
        geo.update_geometry_with_roi(roi);

        REQUIRE(geo.pixels_x() == 50);
        REQUIRE(geo.pixels_y() == 512);
        REQUIRE(geo.modules_x() == 1);
        REQUIRE(geo.modules_y() == 1);
        REQUIRE(geo.get_module_geometries(0).height == 512);
        REQUIRE(geo.get_module_geometries(0).width == 50);
    }
}

TEST_CASE("Two modules side by side") {
    // DetectorGeometry update_geometry_with_roi(DetectorGeometry geo, aare::ROI
    // roi)
    aare::DetectorGeometry geo(aare::xy{1, 2}, 1024, 512);

    REQUIRE(geo.get_module_geometries(0).origin_x == 0);
    REQUIRE(geo.get_module_geometries(0).origin_y == 0);
    REQUIRE(geo.get_module_geometries(0).width == 1024);
    REQUIRE(geo.get_module_geometries(0).height == 512);
    REQUIRE(geo.get_module_geometries(1).origin_x == 1024);
    REQUIRE(geo.get_module_geometries(1).origin_y == 0);

    SECTION("ROI is the whole image") {
        aare::ROI roi;
        roi.xmin = 0;
        roi.xmax = 2048;
        roi.ymin = 0;
        roi.ymax = 512;
        geo.update_geometry_with_roi(roi);

        REQUIRE(geo.pixels_x() == 2048);
        REQUIRE(geo.pixels_y() == 512);
        REQUIRE(geo.modules_x() == 2);
        REQUIRE(geo.modules_y() == 1);
    }
    SECTION("rectangle on both modules") {
        aare::ROI roi;
        roi.xmin = 800;
        roi.xmax = 1300;
        roi.ymin = 200;
        roi.ymax = 499;
        geo.update_geometry_with_roi(roi);

        REQUIRE(geo.pixels_x() == 500);
        REQUIRE(geo.pixels_y() == 299);
        REQUIRE(geo.modules_x() == 2);
        REQUIRE(geo.modules_y() == 1);
        REQUIRE(geo.get_module_geometries(0).height == 299);
        REQUIRE(geo.get_module_geometries(0).width == 224);
        REQUIRE(geo.get_module_geometries(1).height == 299);
        REQUIRE(geo.get_module_geometries(1).width == 276);
    }
}

TEST_CASE("Three modules side by side") {
    // DetectorGeometry update_geometry_with_roi(DetectorGeometry geo, aare::ROI
    // roi)
    aare::DetectorGeometry geo(aare::xy{1, 3}, 1024, 512);
    aare::ROI roi;
    roi.xmin = 700;
    roi.xmax = 2500;
    roi.ymin = 0;
    roi.ymax = 123;

    REQUIRE(geo.get_module_geometries(0).origin_x == 0);
    REQUIRE(geo.get_module_geometries(0).origin_y == 0);
    REQUIRE(geo.get_module_geometries(0).width == 1024);
    REQUIRE(geo.get_module_geometries(0).height == 512);
    REQUIRE(geo.get_module_geometries(1).origin_x == 1024);
    REQUIRE(geo.get_module_geometries(2).origin_x == 2048);

    geo.update_geometry_with_roi(roi);

    REQUIRE(geo.pixels_x() == 1800);
    REQUIRE(geo.pixels_y() == 123);
    REQUIRE(geo.modules_x() == 3);
    REQUIRE(geo.modules_y() == 1);
    REQUIRE(geo.get_module_geometries(0).height == 123);
    REQUIRE(geo.get_module_geometries(0).width == 324);
    REQUIRE(geo.get_module_geometries(1).height == 123);
    REQUIRE(geo.get_module_geometries(1).width == 1024);
    REQUIRE(geo.get_module_geometries(2).height == 123);
    REQUIRE(geo.get_module_geometries(2).width == 452);
}

TEST_CASE("Four modules as a square") {
    // DetectorGeometry update_geometry_with_roi(DetectorGeometry geo, aare::ROI
    // roi)
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

    geo.update_geometry_with_roi(roi);

    REQUIRE(geo.pixels_x() == 1500);
    REQUIRE(geo.pixels_y() == 100);
    REQUIRE(geo.modules_x() == 2);
    REQUIRE(geo.modules_y() == 2);
    REQUIRE(geo.get_module_geometries(0).height == 12);
    REQUIRE(geo.get_module_geometries(0).width == 524);
    REQUIRE(geo.get_module_geometries(1).height == 12);
    REQUIRE(geo.get_module_geometries(1).width == 976);
    REQUIRE(geo.get_module_geometries(2).height == 88);
    REQUIRE(geo.get_module_geometries(2).width == 524);
    REQUIRE(geo.get_module_geometries(3).height == 88);
    REQUIRE(geo.get_module_geometries(3).width == 976);
}