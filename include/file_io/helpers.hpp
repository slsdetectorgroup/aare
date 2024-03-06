#pragma once

#include "file_io/File.hpp"
#include <filesystem>
#include <fmt/core.h>

bool is_master_file(std::filesystem::path fpath);

