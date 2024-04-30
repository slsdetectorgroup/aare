#include "aare/network_io/ZmqHeader.hpp"
#include "aare/utils/logger.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace aare;
TEST_CASE("Test ZmqHeader") {
    ZmqHeader header;
    header.npixelsx = 10;
    header.npixelsy = 15;
    header.data = 1;
    header.jsonversion = 5;
    header.bitmode = 32;
    header.fileIndex = 4;
    header.ndetx = 5;
    header.ndety = 6;
    header.size = 4800;
    header.acqIndex = 8;
    header.frameIndex = 9;
    header.progress = 0.1;
    header.fname = "test";
    header.frameNumber = 11;
    header.expLength = 12;
    header.packetNumber = 13;
    header.detSpec1 = 14;
    header.timestamp = 15;
    header.modId = 16;
    header.row = 17;
    header.column = 18;
    header.detSpec2 = 19;
    header.detSpec3 = 20;
    header.detSpec4 = 21;
    header.detType = 22;
    header.version = 23;
    header.flipRows = 24;
    header.quad = 25;
    header.completeImage = 1;
    header.addJsonHeader = {{"key1", "value1"}, {"key2", "value2"}};
    header.rx_roi = {27, 28, 29, 30};

    std::string json_header = "{"
                              "\"data\": 1, "
                              "\"jsonversion\": 5, "
                              "\"bitmode\": 32, "
                              "\"fileIndex\": 4, "
                              "\"ndetx\": 5, "
                              "\"ndety\": 6, "
                              "\"shape\": [10, 15], "
                              "\"size\": 4800, "
                              "\"acqIndex\": 8, "
                              "\"frameIndex\": 9, "
                              "\"progress\": 0.100000, "
                              "\"fname\": \"test\", "
                              "\"frameNumber\": 11, "
                              "\"expLength\": 12, "
                              "\"packetNumber\": 13, "
                              "\"detSpec1\": 14, "
                              "\"timestamp\": 15, "
                              "\"modId\": 16, "
                              "\"row\": 17, "
                              "\"column\": 18, "
                              "\"detSpec2\": 19, "
                              "\"detSpec3\": 20, "
                              "\"detSpec4\": 21, "
                              "\"detType\": 22, "
                              "\"version\": 23, "
                              "\"flipRows\": 24, "
                              "\"quad\": 25, "
                              "\"completeImage\": 1, "
                              "\"addJsonHeader\": {\"key1\": \"value1\", \"key2\": \"value2\"}, "
                              "\"rx_roi\": [27, 28, 29, 30]"
                              "}";

    SECTION("Test converting ZmqHeader to json string") { REQUIRE(header.to_string() == json_header); }
    SECTION("Test converting json string to ZmqHeader") {
        ZmqHeader header2;
        header2.from_string(json_header);
        REQUIRE(header2 == header);
    }
}
