// SPDX-License-Identifier: MPL-2.0
#include "aare/MultiThreadedFileReader.hpp"

#include "aare/Dtype.hpp"
#include "aare/File.hpp"
#include "aare/FileInterface.hpp"
#include "aare/Frame.hpp"
#include "aare/NumpyFile.hpp"

#include "test_config.hpp"
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

using aare::File;
using aare::FileConfig;
using aare::Frame;
using aare::NumpyFile;
using aare::experimental::MultiThreadedFileReader;

namespace {

class TemporaryNumpyFile {
  public:
    TemporaryNumpyFile() {
        const auto unique =
            std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path() /
                 ("aare-mt-reader-" + std::to_string(unique) + ".npy");

        FileConfig cfg;
        cfg.dtype = aare::Dtype::UINT16;
        cfg.rows = 2;
        cfg.cols = 3;
        NumpyFile file(m_path, "w", cfg);
        for (uint16_t frame_index = 0; frame_index < 10; ++frame_index) {
            Frame frame(cfg.rows, cfg.cols, cfg.dtype);
            auto image = frame.view<uint16_t>();
            for (ssize_t row = 0; row < image.shape(0); ++row) {
                for (ssize_t col = 0; col < image.shape(1); ++col) {
                    image(row, col) = static_cast<uint16_t>(frame_index * 100 +
                                                            row * 10 + col);
                }
            }
            file.write(frame);
        }
    }

    TemporaryNumpyFile(const TemporaryNumpyFile &) = delete;
    TemporaryNumpyFile &operator=(const TemporaryNumpyFile &) = delete;

    ~TemporaryNumpyFile() { std::filesystem::remove(m_path); }

    const std::filesystem::path &path() const { return m_path; }
    void truncate() { std::filesystem::resize_file(m_path, 0); }

  private:
    std::filesystem::path m_path;
};

std::vector<std::byte> read_reference(const std::filesystem::path &fpath,
                                      size_t n_frames) {
    File file(fpath);
    std::vector<std::byte> data(n_frames * file.bytes_per_frame());
    if (n_frames != 0) {
        file.read_into(data.data(), n_frames);
    }
    return data;
}

} // namespace

TEST_CASE("Multi-threaded reader preserves numpy frame order",
          "[MultiThreadedFileReader]") {
    TemporaryNumpyFile file;
    const auto &fpath = file.path();

    MultiThreadedFileReader reader(fpath, 2, 3);

    CHECK(reader.n_threads() == 2);
    CHECK(reader.chunk_size() == 3);
    CHECK(reader.total_frames() == 10);
    CHECK(reader.source_total_frames() == 10);
    CHECK(reader.rows() == 2);
    CHECK(reader.cols() == 3);
    CHECK(reader.bitdepth() == 16);
    CHECK(reader.total_bytes() ==
          reader.total_frames() * reader.bytes_per_frame());

    const auto reference = read_reference(fpath, reader.total_frames());
    auto first = reader.read();
    auto second = reader.read();

    CHECK(first == std::vector<std::byte>(reference.begin(),
                                          reference.begin() + 6 * 12));
    CHECK(second ==
          std::vector<std::byte>(reference.begin() + 6 * 12, reference.end()));
    CHECK(reader.read().empty());
    CHECK(reader.tell() == 10);
    CHECK(reader.remaining_frames() == 0);
}

TEST_CASE("Multi-threaded reader handles uneven raw chunks and frame limits",
          "[.with-data][MultiThreadedFileReader]") {
    const auto fpath =
        test_data_path() / "raw/jungfrau/jungfrau_single_master_0.json";
    REQUIRE(std::filesystem::exists(fpath));

    MultiThreadedFileReader reader(fpath, 2, 3, 9);

    CHECK(reader.total_frames() == 9);
    CHECK(reader.source_total_frames() == 10);
    CHECK(reader.read_all() == read_reference(fpath, 9));
}

TEST_CASE("Multi-threaded reader can seek and reread",
          "[MultiThreadedFileReader]") {
    TemporaryNumpyFile file;

    MultiThreadedFileReader reader(file.path(), 2, 1);
    const auto first = reader.read();
    CHECK(reader.tell() == 2);
    CHECK(reader.next_read_frames() == 2);

    reader.seek(0);
    CHECK(reader.tell() == 0);
    CHECK(reader.read() == first);
    CHECK_THROWS_AS(reader.seek(11), std::out_of_range);
}

TEST_CASE("Multi-threaded reader validates its configuration",
          "[MultiThreadedFileReader]") {
    TemporaryNumpyFile file;
    const auto &fpath = file.path();

    CHECK_THROWS_AS(MultiThreadedFileReader(fpath, 0, 1),
                    std::invalid_argument);
    CHECK_THROWS_AS(MultiThreadedFileReader(fpath, 1, 0),
                    std::invalid_argument);
    CHECK_THROWS_AS(MultiThreadedFileReader(fpath, 1, 1, 11),
                    std::invalid_argument);

    MultiThreadedFileReader reader(fpath, 2, 2);
    CHECK_THROWS_AS(reader.read_into(nullptr), std::invalid_argument);
}

TEST_CASE("An explicit zero frame limit produces an empty read",
          "[MultiThreadedFileReader]") {
    TemporaryNumpyFile file;

    MultiThreadedFileReader reader(file.path(), 8, 3, 0);
    CHECK(reader.total_frames() == 0);
    CHECK(reader.total_bytes() == 0);
    CHECK(reader.read().empty());
    CHECK_NOTHROW(reader.read_into(nullptr));
}

TEST_CASE("read_into reads at most one chunk per worker",
          "[MultiThreadedFileReader]") {
    TemporaryNumpyFile file;
    MultiThreadedFileReader reader(file.path(), 3, 2);
    std::vector<std::byte> data(reader.next_read_bytes());

    CHECK(reader.next_read_frames() == 6);
    CHECK(reader.read_into(data.data()) == 6);
    CHECK(reader.tell() == 6);
    CHECK(reader.next_read_frames() == 4);
    CHECK(reader.next_read_bytes() == 4 * reader.bytes_per_frame());
}

TEST_CASE("Multi-threaded reader propagates worker errors",
          "[MultiThreadedFileReader]") {
    TemporaryNumpyFile file;
    MultiThreadedFileReader reader(file.path(), 3, 2);
    file.truncate();

    CHECK_THROWS(reader.read());
}

TEST_CASE("Multi-threaded reader can close its worker files",
          "[MultiThreadedFileReader]") {
    TemporaryNumpyFile file;
    MultiThreadedFileReader reader(file.path(), 2, 2);

    CHECK(reader.is_open());
    reader.close();
    CHECK_FALSE(reader.is_open());
    CHECK_NOTHROW(reader.close());
    CHECK_THROWS_AS(reader.read(), std::runtime_error);
    CHECK_THROWS_AS(reader.read_all(), std::runtime_error);
    CHECK_THROWS_AS(reader.seek(0), std::runtime_error);
}
