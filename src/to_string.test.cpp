#include "aare/to_string.hpp"

#include <catch2/catch_test_macros.hpp>

using aare::StringTo;
using aare::ToString;

TEST_CASE("Detector Type to string conversion") {
    // TODO! By the way I don't think the enum string conversions should be in
    // the defs.hpp file but let's use this to show a test
    REQUIRE(ToString(aare::DetectorType::Generic) == "Generic");
    REQUIRE(ToString(aare::DetectorType::Eiger) == "Eiger");
    REQUIRE(ToString(aare::DetectorType::Gotthard) == "Gotthard");
    REQUIRE(ToString(aare::DetectorType::Jungfrau) == "Jungfrau");
    REQUIRE(ToString(aare::DetectorType::ChipTestBoard) == "ChipTestBoard");
    REQUIRE(ToString(aare::DetectorType::Moench) == "Moench");
    REQUIRE(ToString(aare::DetectorType::Mythen3) == "Mythen3");
    REQUIRE(ToString(aare::DetectorType::Gotthard2) == "Gotthard2");
    REQUIRE(ToString(aare::DetectorType::Xilinx_ChipTestBoard) ==
            "Xilinx_ChipTestBoard");
    REQUIRE(ToString(aare::DetectorType::Moench03) == "Moench03");
    REQUIRE(ToString(aare::DetectorType::Moench03_old) == "Moench03_old");
    REQUIRE(ToString(aare::DetectorType::Unknown) == "Unknown");
}

TEST_CASE("String to Detector Type") {
    REQUIRE(StringTo<aare::DetectorType>("Generic") ==
            aare::DetectorType::Generic);
    REQUIRE(StringTo<aare::DetectorType>("Eiger") == aare::DetectorType::Eiger);
    REQUIRE(StringTo<aare::DetectorType>("Gotthard") ==
            aare::DetectorType::Gotthard);
    REQUIRE(StringTo<aare::DetectorType>("Jungfrau") ==
            aare::DetectorType::Jungfrau);
    REQUIRE(StringTo<aare::DetectorType>("ChipTestBoard") ==
            aare::DetectorType::ChipTestBoard);
    REQUIRE(StringTo<aare::DetectorType>("Moench") ==
            aare::DetectorType::Moench);
    REQUIRE(StringTo<aare::DetectorType>("Mythen3") ==
            aare::DetectorType::Mythen3);
    REQUIRE(StringTo<aare::DetectorType>("Gotthard2") ==
            aare::DetectorType::Gotthard2);
    REQUIRE(StringTo<aare::DetectorType>("Xilinx_ChipTestBoard") ==
            aare::DetectorType::Xilinx_ChipTestBoard);
    REQUIRE(StringTo<aare::DetectorType>("Moench03") ==
            aare::DetectorType::Moench03);
    REQUIRE(StringTo<aare::DetectorType>("Moench03_old") ==
            aare::DetectorType::Moench03_old);
    REQUIRE(StringTo<aare::DetectorType>("Unknown") ==
            aare::DetectorType::Unknown);
}

TEST_CASE("conversion from duration to string") {
    using ns = std::chrono::nanoseconds;
    using us = std::chrono::microseconds;
    using ms = std::chrono::milliseconds;
    using s = std::chrono::seconds;
    REQUIRE(ToString(ns(150)) == "150ns");
    REQUIRE(ToString(ms(783)) == "0.783s");
    REQUIRE(ToString(ms(783), "ms") == "783ms");
    REQUIRE(ToString(us(0)) == "0ns"); // Defaults to the lowest unit
    REQUIRE(ToString(us(0), "s") == "0s");
    REQUIRE(ToString(s(-1)) == "-1s");
    REQUIRE(ToString(us(-100)) == "-100us");
}

TEST_CASE("Convert vector of time") {
    using ns = std::chrono::nanoseconds;
    using us = std::chrono::microseconds;
    using ms = std::chrono::milliseconds;
    using s = std::chrono::seconds;
    std::vector<ns> vec{ns(150), us(10), ns(600)};
    REQUIRE(ToString(vec) == "[150ns, 10us, 600ns]");
    vec[0] = ms(150);
    vec[1] = s(10);
    REQUIRE(ToString(vec) == "[0.15s, 10s, 600ns]");
    // REQUIRE(ToString(vec, "ns") == "[150ns, 10000ns, 600ns]");
}

TEST_CASE("string to std::chrono::duration") {
    using ns = std::chrono::nanoseconds;
    using us = std::chrono::microseconds;
    using ms = std::chrono::milliseconds;
    using s = std::chrono::seconds;
    REQUIRE(StringTo<ns>("150", "ns") == ns(150));
    REQUIRE(StringTo<ns>("150us") == us(150));
    REQUIRE(StringTo<ns>("150ms") == ms(150));
    REQUIRE(StringTo<s>("3 s") == s(3));
    REQUIRE_THROWS(StringTo<ns>("5xs"));
    REQUIRE_THROWS(StringTo<ns>("asvn"));
}

TEST_CASE("Vector of int") {
    std::vector<int> vec;
    REQUIRE(ToString(vec) == "[]");

    vec.push_back(1);
    REQUIRE(ToString(vec) == "[1]");

    vec.push_back(172);
    REQUIRE(ToString(vec) == "[1, 172]");

    vec.push_back(5000);
    REQUIRE(ToString(vec) == "[1, 172, 5000]");
}

TEST_CASE("Vector of double") {
    std::vector<double> vec;
    REQUIRE(ToString(vec) == "[]");

    vec.push_back(1.3);
    REQUIRE(ToString(vec) == "[1.3]");

    // vec.push_back(5669.325);
    // REQUIRE(ToString(vec) == "[1.3, 5669.325]");

    // vec.push_back(-5669.325005);
    // REQUIRE(ToString(vec) == "[1.3, 5669.325, -5669.325005]");
}

TEST_CASE("String to string") {
    std::string k = "hej";
    REQUIRE(ToString(k) == "hej");
}

TEST_CASE("vector of strings") {
    std::vector<std::string> vec{"5", "s"};
    REQUIRE(ToString(vec) == "[5, s]");

    std::vector<std::string> vec2{"some", "strange", "words", "75"};
    REQUIRE(ToString(vec2) == "[some, strange, words, 75]");
}

TEST_CASE("uint32 from string") {
    REQUIRE(StringTo<uint32_t>("0") == 0);
    REQUIRE(StringTo<uint32_t>("5") == 5u);
    REQUIRE(StringTo<uint32_t>("16") == 16u);
    REQUIRE(StringTo<uint32_t>("20") == 20u);
    REQUIRE(StringTo<uint32_t>("0x14") == 20u);
    REQUIRE(StringTo<uint32_t>("0x15") == 21u);
    REQUIRE(StringTo<uint32_t>("0x15") == 0x15);
    REQUIRE(StringTo<uint32_t>("0xffffff") == 0xffffff);
}

TEST_CASE("uint64 from string") {
    REQUIRE(StringTo<uint64_t>("0") == 0);
    REQUIRE(StringTo<uint64_t>("5") == 5u);
    REQUIRE(StringTo<uint64_t>("16") == 16u);
    REQUIRE(StringTo<uint64_t>("20") == 20u);
    REQUIRE(StringTo<uint64_t>("0x14") == 20u);
    REQUIRE(StringTo<uint64_t>("0x15") == 21u);
    REQUIRE(StringTo<uint64_t>("0xffffff") == 0xffffff);
}

TEST_CASE("int from string") {
    REQUIRE(StringTo<int>("-1") == -1);
    REQUIRE(StringTo<int>("-0x1") == -0x1);
    REQUIRE(StringTo<int>("-0x1") == -1);
    REQUIRE(StringTo<int>("0") == 0);
    REQUIRE(StringTo<int>("5") == 5);
    REQUIRE(StringTo<int>("16") == 16);
    REQUIRE(StringTo<int>("20") == 20);
    REQUIRE(StringTo<int>("0x14") == 20);
    REQUIRE(StringTo<int>("0x15") == 21);
    REQUIRE(StringTo<int>("0xffffff") == 0xffffff);
}

TEST_CASE("std::map of strings") {
    std::map<std::string, std::string> m;
    m["key"] = "value";
    auto s = ToString(m);
    REQUIRE(s == "{key: value}");

    m["chrusi"] = "musi";
    REQUIRE(ToString(m) == "{chrusi: musi, key: value}");

    m["test"] = "tree";
    REQUIRE(ToString(m) == "{chrusi: musi, key: value, test: tree}");
}

TEST_CASE("Formatting ROI") {
    aare::ROI roi;
    roi.xmin = 5;
    roi.xmax = 159;
    roi.ymin = 6;
    roi.ymax = 170;
    REQUIRE(ToString(roi) == "[5, 159, 6, 170]");
}

TEST_CASE("Streaming of ROI") {
    aare::ROI roi;
    roi.xmin = 50;
    roi.xmax = 109;
    roi.ymin = 6;
    roi.ymax = 130;
    std::ostringstream oss;
    oss << roi;
    REQUIRE(oss.str() == "[50, 109, 6, 130]");
}

// TODO: After StringTo<scanParameters> is implemented
/*TEST_CASE("Streaming of scanParameters") {
    using namespace sls;
    {
        aare::scanParameters t{};
        std::ostringstream oss;
        oss << t;
        REQUIRE(oss.str() == "[disabled]");
    }
    {
        aare::scanParameters t{defs::VTH2, 500, 1500, 500};
        std::ostringstream oss;
        oss << t;
        REQUIRE(oss.str() == "[enabled\ndac vth2\nstart 500\nstop 1500\nstep "
                             "500\nsettleTime 1ms\n]");
    }
    {
        aare::scanParameters t{defs::VTH2, 500, 1500, 500,
                               std::chrono::milliseconds{500}};
        std::ostringstream oss;
        oss << t;
        REQUIRE(oss.str() == "[enabled\ndac vth2\nstart 500\nstop 1500\nstep "
                             "500\nsettleTime 0.5s\n]");
    }
}*/
