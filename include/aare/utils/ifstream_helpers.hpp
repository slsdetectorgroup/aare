#pragma once

#include <fstream>
#include <string>
namespace aare {

/**
 * @brief Get the error message from an ifstream object
 */
std::string ifstream_error_msg(std::ifstream &ifs);

} // namespace aare