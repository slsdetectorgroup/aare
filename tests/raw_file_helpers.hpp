// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "aare/defs.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

class TemporaryRawFiles {
    std::filesystem::path m_directory;

  public:
    explicit TemporaryRawFiles(size_t modules = 1) {
        const auto unique =
            std::chrono::steady_clock::now().time_since_epoch().count();
        m_directory = std::filesystem::temp_directory_path() /
                      ("aare-raw-" + std::to_string(unique));
        std::filesystem::create_directory(m_directory);
        const nlohmann::json metadata = {
            {"Version", 7.2},
            {"Detector Type", "Jungfrau"},
            {"Timing Mode", "auto"},
            {"Geometry", {{"x", 1}, {"y", modules}}},
            {"Image Size in bytes", 12},
            {"Pixels", {{"x", 3}, {"y", 2}}},
            {"Max Frames Per File", 1},
            {"Total Frames", 2},
            {"Frames in File", 2},
            {"Frame Padding", 1},
            {"Frame Discard Policy", "nodiscard"}};
        std::ofstream(master_path()) << metadata;
        for (size_t module = 0; module < modules; ++module) {
            for (size_t file = 0; file < 2; ++file) {
                std::ofstream output(data_path(module, file), std::ios::binary);
                aare::DetectorHeader header{};
                header.frameNumber = 100 + file;
                const uint16_t pixels[6] = {1, 2, 3, 4, 5, 6};
                output.write(reinterpret_cast<const char *>(&header),
                             sizeof(header));
                output.write(reinterpret_cast<const char *>(pixels),
                             sizeof(pixels));
            }
        }
    }

    TemporaryRawFiles(const TemporaryRawFiles &) = delete;
    TemporaryRawFiles &operator=(const TemporaryRawFiles &) = delete;
    ~TemporaryRawFiles() { std::filesystem::remove_all(m_directory); }

    std::filesystem::path master_path() const {
        return m_directory / "run_master_0.json";
    }

    std::filesystem::path data_path(size_t module = 0, size_t file = 0) const {
        return m_directory / ("run_d" + std::to_string(module) + "_f" +
                              std::to_string(file) + "_0.raw");
    }
};
