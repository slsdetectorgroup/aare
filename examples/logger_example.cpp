#include "aare/utils/logger.hpp"
#include <fstream>
#include <iostream>

int main() {
    aare::logger::debug(LOCATION, "hello", 1, "world", std::vector<long>{1, 2, 3, 4, 5});
    aare::logger::debug(LOCATION, "setting verbosity to INFO");
    aare::logger::set_verbosity(aare::logger::INFO);
    aare::logger::debug(LOCATION, "NOTHING SHOULD BE PRINTED");
    aare::logger::info(LOCATION, "info printed");

    // writing to file
    std::ofstream textfile;
    textfile.open("Test.txt");
    aare::logger::set_streams(textfile.rdbuf());
    aare::logger::info(LOCATION, "info printed to file");

    // writing with a local logger instance
    aare::logger::Logger logger;
    logger.set_verbosity(aare::logger::WARNING);
    logger.debug(LOCATION, "NOTHING SHOULD BE PRINTED");
    logger.info(LOCATION, "NOTHING SHOULD BE PRINTED");
    logger.warn(LOCATION, "warning printed in std::cout");
    aare::logger::info(LOCATION, "info printed in file ##");
    textfile.close();

    // setting file output by path
    // user doesn't have to close file
    aare::logger::set_output_file("Test2.txt");
    aare::logger::info(LOCATION, "info printed to Test2.txt");
    return 0;
}