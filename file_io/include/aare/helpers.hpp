#pragma once

#include "aare/FileInterface.hpp"
#include <filesystem>
#include <fmt/core.h>

namespace aare {

bool is_master_file(std::filesystem::path fpath);

}