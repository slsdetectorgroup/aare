// SPDX-License-Identifier: MPL-2.0
#include "aare/logger.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <iostream>
#include <sstream>
#include <string>

using Catch::Matchers::EndsWith;
using Catch::Matchers::Matches;
using Catch::Matchers::StartsWith;

TEST_CASE("Log timestamp is HH:MM:SS.mmm", "[logger]") {
    const std::string ts = aare::Logger::Timestamp();
    REQUIRE_THAT(ts, Matches(R"(\d{2}:\d{2}:\d{2}\.\d{3})"));
}

TEST_CASE("short_file_name strips both kinds of directory separators",
          "[logger]") {
    REQUIRE(std::string(aare::short_file_name("/a/b/File.cpp")) == "File.cpp");
    REQUIRE(std::string(aare::short_file_name("C:\\a\\b\\File.cpp")) ==
            "File.cpp");
    REQUIRE(std::string(aare::short_file_name("File.cpp")) == "File.cpp");
}

TEST_CASE("LOG writes one coloured line with timestamp and level", "[logger]") {
    std::ostringstream captured;
    auto *previous = std::clog.rdbuf(captured.rdbuf());
    LOG(aare::logWARNING) << "hello " << 42;
    std::clog.rdbuf(previous);

    const std::string line = captured.str();
    // <colour>- HH:MM:SS.mmm WARNING: hello 42<reset>\n
    REQUIRE_THAT(line, StartsWith(std::string(YELLOW BOLD) + "- "));
    REQUIRE_THAT(line, Matches(std::string("\x1b\\[33m\x1b\\[1m- ") +
                               R"(\d{2}:\d{2}:\d{2}\.\d{3} WARNING: hello 42)" +
                               "\x1b\\[0m\n"));
    REQUIRE_THAT(line, EndsWith(std::string(RESET) + "\n"));
}
