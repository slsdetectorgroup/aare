// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <atomic>
#include <filesystem>
#include <random>
#include <string>

/**
 * A path in the temporary directory that no other test uses. ctest runs the
 * test cases as separate processes at the same time, so a name built from the
 * clock alone can be handed to two of them; the random number differs between
 * processes and the counter within one.
 */
inline std::filesystem::path unique_temporary_path(const std::string &prefix,
                                                   const std::string &suffix) {
    static const auto process = std::random_device{}();
    static std::atomic<unsigned> counter{0};
    return std::filesystem::temp_directory_path() /
           (prefix + "-" + std::to_string(process) + "-" +
            std::to_string(counter++) + suffix);
}
