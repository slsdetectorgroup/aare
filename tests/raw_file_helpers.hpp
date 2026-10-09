// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "aare/defs.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

/**
 * @brief Throwaway raw file series with a 2x3 pixel Jungfrau "module" per
 * data file. Modules are laid out as DetectorGeometry orders them (module
 * index runs down each column first) and every module holds distinct pixel
 * values so that misplaced modules are detected.
 */
class TemporaryRawFiles {
    std::filesystem::path m_directory;
    aare::xy m_layout;
    aare::xy m_module_size;

  public:
    static constexpr size_t module_rows = 2;
    static constexpr size_t module_cols = 3;
    static constexpr size_t frames = 2;

    /// Value written to pixel index `pixel` (row major) of module `module`
    static uint16_t pixel_value(size_t module, size_t pixel) {
        return static_cast<uint16_t>((module * 10 + pixel + 1) % 65536);
    }

    /// Modules stacked in one column
    explicit TemporaryRawFiles(size_t modules = 1)
        : TemporaryRawFiles(aare::xy{static_cast<uint32_t>(modules), 1}) {}

    /// Modules of `module_size` pixels arranged in `layout.row` rows and
    /// `layout.col` columns. The default module is 2x3 pixels; pass
    /// {512, 1024} for a full Jungfrau module.
    explicit TemporaryRawFiles(aare::xy layout,
                               aare::xy module_size = {module_rows,
                                                       module_cols})
        : m_layout(layout), m_module_size(module_size) {
        const auto unique =
            std::chrono::steady_clock::now().time_since_epoch().count();
        m_directory = std::filesystem::temp_directory_path() /
                      ("aare-raw-" + std::to_string(unique));
        std::filesystem::create_directory(m_directory);
        const size_t pixels_per_module = module_size.row * module_size.col;
        const nlohmann::json metadata = {
            {"Version", 7.2},
            {"Detector Type", "Jungfrau"},
            {"Timing Mode", "auto"},
            {"Geometry", {{"x", layout.col}, {"y", layout.row}}},
            {"Image Size in bytes", pixels_per_module * 2},
            {"Pixels", {{"x", module_size.col}, {"y", module_size.row}}},
            {"Max Frames Per File", 1},
            {"Total Frames", frames},
            {"Frames in File", frames},
            {"Frame Padding", 1},
            {"Frame Discard Policy", "nodiscard"}};
        std::ofstream(master_path()) << metadata;
        std::vector<uint16_t> pixels(pixels_per_module);
        for (size_t module = 0; module < n_modules(); ++module) {
            for (size_t pixel = 0; pixel < pixels_per_module; ++pixel) {
                pixels[pixel] = pixel_value(module, pixel);
            }
            for (size_t file = 0; file < frames; ++file) {
                std::ofstream output(data_path(module, file), std::ios::binary);
                aare::DetectorHeader header{};
                header.frameNumber = 100 + file;
                header.row = static_cast<uint16_t>(module % layout.row);
                header.column = static_cast<uint16_t>(module / layout.row);
                output.write(reinterpret_cast<const char *>(&header),
                             sizeof(header));
                output.write(reinterpret_cast<const char *>(pixels.data()),
                             static_cast<std::streamsize>(pixels.size() *
                                                          sizeof(uint16_t)));
            }
        }
    }

    aare::xy layout() const { return m_layout; }
    aare::xy module_size() const { return m_module_size; }
    size_t n_modules() const { return m_layout.row * m_layout.col; }
    size_t rows() const { return m_module_size.row * m_layout.row; }
    size_t cols() const { return m_module_size.col * m_layout.col; }

    /// Module index (data file number) of the module at a layout position
    size_t module_index(size_t module_row, size_t module_col) const {
        return module_col * m_layout.row + module_row;
    }

    /// Expected value of pixel (row, col) in the assembled image
    uint16_t expected(size_t row, size_t col) const {
        const size_t module =
            module_index(row / m_module_size.row, col / m_module_size.col);
        const size_t pixel = (row % m_module_size.row) * m_module_size.col +
                             col % m_module_size.col;
        return pixel_value(module, pixel);
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
