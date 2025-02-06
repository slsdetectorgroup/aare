#include "aare/File.hpp"
#include "aare/RawMasterFile.hpp" //needed for ROI
#include "aare/RawFile.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>

#include "aare/geo_helpers.hpp"
#include "test_config.hpp"

TEST_CASE("Simple ROIs on one module"){
    // DetectorGeometry geometry_from_roi(DetectorGeometry geo, aare::ROI roi)
    aare::DetectorGeometry geo;
    aare::ROI roi;
    roi.xmin = 100;
    roi.xmax = 200;
    roi.ymin = 150;
    roi.ymax = 200;

    aare::ModuleGeometry mod; 
    mod.x = 0;
    mod.y = 0;
    mod.width = 1024;
    mod.height = 512;

    geo.pixels_x = 1024;
    geo.pixels_y = 512;
    geo.modules_x = 1;
    geo.modules_y = 1;
    geo.module_pixel_0.push_back(mod);

    auto updated_geo = aare::geometry_from_roi(geo, roi);

    REQUIRE(updated_geo.pixels_x == 100);
    REQUIRE(updated_geo.pixels_y == 50);
    REQUIRE(updated_geo.modules_x == 1);
    REQUIRE(updated_geo.modules_y == 1);
}

TEST_CASE("One pixel wide ROI on one module"){
    // DetectorGeometry geometry_from_roi(DetectorGeometry geo, aare::ROI roi)
    aare::DetectorGeometry geo;
    aare::ROI roi;
    roi.xmin = 0;
    roi.xmax = 700;
    roi.ymin = 0;
    roi.ymax = 1;

    aare::ModuleGeometry mod; 
    mod.x = 0;
    mod.y = 0;
    mod.width = 1024;
    mod.height = 512;

    geo.pixels_x = 1024;
    geo.pixels_y = 512;
    geo.modules_x = 1;
    geo.modules_y = 1;
    geo.module_pixel_0.push_back(mod);

    auto updated_geo = aare::geometry_from_roi(geo, roi);

    REQUIRE(updated_geo.pixels_x == 700);
    REQUIRE(updated_geo.pixels_y == 1);
    REQUIRE(updated_geo.modules_x == 1);
    REQUIRE(updated_geo.modules_y == 1);
}

TEST_CASE("Two modules side by side"){
    // DetectorGeometry geometry_from_roi(DetectorGeometry geo, aare::ROI roi)
    aare::DetectorGeometry geo;
    aare::ROI roi;
    roi.xmin = 800;
    roi.xmax = 1300;
    roi.ymin = 200;
    roi.ymax = 499;

    aare::ModuleGeometry mod; 
    mod.x = 0;
    mod.y = 0;
    mod.width = 1024;
    mod.height = 512;

    geo.pixels_x = 2048;
    geo.pixels_y = 512;
    geo.modules_x = 2;
    geo.modules_y = 1;

    geo.module_pixel_0.push_back(mod);
    mod.x = 1024;
    geo.module_pixel_0.push_back(mod);

    auto updated_geo = aare::geometry_from_roi(geo, roi);

    REQUIRE(updated_geo.pixels_x == 500);
    REQUIRE(updated_geo.pixels_y == 299);
    REQUIRE(updated_geo.modules_x == 2);
    REQUIRE(updated_geo.modules_y == 1);
    REQUIRE(updated_geo.module_pixel_0[0].height == 299);
    REQUIRE(updated_geo.module_pixel_0[0].width == 224);
    REQUIRE(updated_geo.module_pixel_0[1].height == 299);
    REQUIRE(updated_geo.module_pixel_0[1].width == 276);
}

TEST_CASE("Three modules side by side"){
    // DetectorGeometry geometry_from_roi(DetectorGeometry geo, aare::ROI roi)
    aare::DetectorGeometry geo;
    aare::ROI roi;
    roi.xmin = 700;
    roi.xmax = 2500;
    roi.ymin = 0;
    roi.ymax = 123;

    aare::ModuleGeometry mod; 
    mod.x = 0;
    mod.y = 0;
    mod.width = 1024;
    mod.height = 512;

    geo.pixels_x = 3072;
    geo.pixels_y = 512;
    geo.modules_x = 3;
    geo.modules_y = 1;

    geo.module_pixel_0.push_back(mod);
    mod.x = 1024;
    geo.module_pixel_0.push_back(mod);
    mod.x = 2048;
    geo.module_pixel_0.push_back(mod);

    auto updated_geo = aare::geometry_from_roi(geo, roi);

    REQUIRE(updated_geo.pixels_x == 1800);
    REQUIRE(updated_geo.pixels_y == 123);
    REQUIRE(updated_geo.modules_x == 3);
    REQUIRE(updated_geo.modules_y == 1);
    REQUIRE(updated_geo.module_pixel_0[0].height == 123);
    REQUIRE(updated_geo.module_pixel_0[0].width == 324);
    REQUIRE(updated_geo.module_pixel_0[1].height == 123);
    REQUIRE(updated_geo.module_pixel_0[1].width == 1024);
    REQUIRE(updated_geo.module_pixel_0[2].height == 123);
    REQUIRE(updated_geo.module_pixel_0[2].width == 452);
}

TEST_CASE("Four modules as a square"){
    // DetectorGeometry geometry_from_roi(DetectorGeometry geo, aare::ROI roi)
    aare::DetectorGeometry geo;
    aare::ROI roi;
    roi.xmin = 500;
    roi.xmax = 2000;
    roi.ymin = 500;
    roi.ymax = 600;

    aare::ModuleGeometry mod; 
    mod.x = 0;
    mod.y = 0;
    mod.width = 1024;
    mod.height = 512;

    geo.pixels_x = 2048;
    geo.pixels_y = 1024;
    geo.modules_x = 2;
    geo.modules_y = 2;

    geo.module_pixel_0.push_back(mod);
    mod.x = 1024;
    geo.module_pixel_0.push_back(mod);
    mod.x = 0;
    mod.y = 512;
    geo.module_pixel_0.push_back(mod);
    mod.x = 1024;
    geo.module_pixel_0.push_back(mod);

    auto updated_geo = aare::geometry_from_roi(geo, roi);

    REQUIRE(updated_geo.pixels_x == 1500);
    REQUIRE(updated_geo.pixels_y == 100);
    REQUIRE(updated_geo.modules_x == 2);
    REQUIRE(updated_geo.modules_y == 2);
    REQUIRE(updated_geo.module_pixel_0[0].height == 12);
    REQUIRE(updated_geo.module_pixel_0[0].width == 524);
    REQUIRE(updated_geo.module_pixel_0[1].height == 12);
    REQUIRE(updated_geo.module_pixel_0[1].width == 976);
    REQUIRE(updated_geo.module_pixel_0[2].height == 88);
    REQUIRE(updated_geo.module_pixel_0[2].width == 524);
    REQUIRE(updated_geo.module_pixel_0[3].height == 88);
    REQUIRE(updated_geo.module_pixel_0[3].width == 976);
}