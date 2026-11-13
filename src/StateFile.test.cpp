// SPDX-License-Identifier: MPL-2.0
#include "aare/StateFile.hpp"

#include "aare/FastPedestal.hpp"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <nlohmann/json.hpp>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <sstream>

using namespace aare;
using Catch::Matchers::ContainsSubstring;

namespace {

constexpr size_t state_file_preamble_size = 16;

// A path in the temporary directory, removed again with its temporary file
class TemporaryStateFile {
  public:
    TemporaryStateFile() {
        static std::atomic<int> counter{0};
        const auto unique =
            std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path() /
                 fmt::format("aare-state-file-{}-{}.aare", unique, counter++);
    }

    TemporaryStateFile(const TemporaryStateFile &) = delete;
    TemporaryStateFile &operator=(const TemporaryStateFile &) = delete;

    ~TemporaryStateFile() {
        std::error_code error;
        std::filesystem::remove(m_path, error);
        std::filesystem::remove(tmp_path(), error);
    }

    const std::filesystem::path &path() const { return m_path; }

    // The name save_state() writes to before renaming
    std::filesystem::path tmp_path() const {
        auto tmp = m_path;
        tmp += ".tmp";
        return tmp;
    }

  private:
    std::filesystem::path m_path;
};

// A state with values that only survive an exact copy of their bytes. Its
// dtype differs from that of the arrays, which are always double
FastPedestalState make_test_state() {
    FastPedestalState state;
    state.dtype = Dtype::FLOAT;
    state.n_samples = 5;
    state.cur_samples = 3;
    state.sum = NDArray<double, 2>({2, 3});
    state.sum2 = NDArray<double, 2>({2, 3});
    const std::array<double, 6> values{
        0.1,
        -0.0,
        1e300,
        std::numeric_limits<double>::denorm_min(),
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity()};
    for (ssize_t i = 0; i < 6; ++i) {
        state.sum[i] = values[static_cast<size_t>(i)];
        state.sum2[i] = static_cast<double>(i + 1) / 3.0;
    }
    return state;
}

bool same_bits(const NDArray<double, 2> &a, const NDArray<double, 2> &b) {
    return a.shape() == b.shape() &&
           std::memcmp(a.data(), b.data(), a.total_bytes()) == 0;
}

bool same_state(const FastPedestalState &a, const FastPedestalState &b) {
    return a.dtype == b.dtype && a.n_samples == b.n_samples &&
           a.cur_samples == b.cur_samples && same_bits(a.sum, b.sum) &&
           same_bits(a.sum2, b.sum2);
}

// The dtype string a state file records for a pedestal type
template <typename T> std::string pedestal_dtype_string() {
    if constexpr (std::is_same_v<T, double>)
        return "<f8";
    else if constexpr (std::is_same_v<T, float>)
        return "<f4";
    else
        return "<i2";
}

std::string read_bytes(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream bytes;
    bytes << in.rdbuf();
    return bytes.str();
}

uint64_t header_length(const std::string &bytes) {
    uint64_t length = 0;
    std::memcpy(&length, bytes.data() + 8, sizeof(length));
    return length;
}

// Write a state file with any header text, to test what the reader rejects
void write_state_file(const std::filesystem::path &path,
                      const std::string &header, const std::string &data,
                      const std::string &magic = "AARESTAT") {
    const uint64_t length = header.size();
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(magic.data(), static_cast<std::streamsize>(magic.size()));
    out.write(reinterpret_cast<const char *>(&length), sizeof(length));
    out.write(header.data(), static_cast<std::streamsize>(header.size()));
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
}

// Rewrite the header of a state file, keeping its data
void modify_header(const std::filesystem::path &path,
                   const std::function<void(nlohmann::json &)> &modify) {
    const auto bytes = read_bytes(path);
    const auto data_start = state_file_preamble_size + header_length(bytes);
    auto header = nlohmann::json::parse(
        bytes.substr(state_file_preamble_size, header_length(bytes)));
    modify(header);
    write_state_file(path, header.dump(), bytes.substr(data_start));
}

} // namespace

TEST_CASE("state file returns a FastPedestalState bit for bit",
          "[state-file]") {
    TemporaryStateFile file;
    const auto state = make_test_state();
    save_state(state, file.path());

    const auto loaded = load_state<FastPedestalState>(file.path());
    REQUIRE(loaded.dtype == Dtype::FLOAT);
    REQUIRE(loaded.n_samples == 5);
    REQUIRE(loaded.cur_samples == 3);
    REQUIRE(loaded.sum.shape() == Shape<2>{2, 3});
    REQUIRE(loaded.sum2.shape() == Shape<2>{2, 3});
    REQUIRE(same_bits(loaded.sum, state.sum));
    REQUIRE(same_bits(loaded.sum2, state.sum2));
}

TEST_CASE("state file has the documented layout", "[state-file]") {
    TemporaryStateFile file;
    const auto state = make_test_state();
    save_state(state, file.path());
    const auto bytes = read_bytes(file.path());

    REQUIRE(bytes.substr(0, 8) == "AARESTAT");
    const auto data_start = state_file_preamble_size + header_length(bytes);
    REQUIRE(data_start % 64 == 0);

    const auto header = nlohmann::json::parse(
        bytes.substr(state_file_preamble_size, header_length(bytes)));
    REQUIRE(header.at("format_version") == 1);
    REQUIRE(header.at("type") == "FastPedestal");
    REQUIRE(header.at("aare_version").is_string());
    REQUIRE_FALSE(header.at("aare_version").get<std::string>().empty());

    const auto &fields = header.at("state");
    REQUIRE(fields.at("dtype") == "<f4");
    REQUIRE(fields.at("n_samples") == 5);
    REQUIRE(fields.at("cur_samples") == 3);
    // Each array of 2 * 3 doubles starts on the next multiple of 64 bytes
    const size_t array_bytes = 2 * 3 * sizeof(double);
    const nlohmann::json expected_shape{2, 3};
    REQUIRE(fields.at("sum").at("dtype") == "<f8");
    REQUIRE(fields.at("sum").at("shape") == expected_shape);
    REQUIRE(fields.at("sum").at("offset") == 0);
    REQUIRE(fields.at("sum2").at("dtype") == "<f8");
    REQUIRE(fields.at("sum2").at("shape") == expected_shape);
    REQUIRE(fields.at("sum2").at("offset") == 64);

    REQUIRE(bytes.size() == data_start + 64 + array_bytes);
    REQUIRE(std::memcmp(bytes.data() + data_start, state.sum.data(),
                        array_bytes) == 0);
    REQUIRE(std::memcmp(bytes.data() + data_start + 64, state.sum2.data(),
                        array_bytes) == 0);
}

TEST_CASE("state file is replaced as a whole", "[state-file]") {
    TemporaryStateFile file;
    auto state = make_test_state();
    save_state(state, file.path());

    state.cur_samples = 4;
    state.sum[0] = 42.0;
    save_state(state, file.path());

    REQUIRE(same_state(load_state<FastPedestalState>(file.path()), state));
    REQUIRE_FALSE(std::filesystem::exists(file.tmp_path()));
}

TEST_CASE("state file that cannot be written reports it", "[state-file]") {
    TemporaryStateFile directory;
    const auto path = directory.path() / "missing-directory" / "state.aare";
    REQUIRE_THROWS_AS(save_state(make_test_state(), path), std::runtime_error);
    REQUIRE_FALSE(std::filesystem::exists(path));
}

TEST_CASE("state file needs the dtype of the state", "[state-file]") {
    TemporaryStateFile file;
    auto state = make_test_state();
    state.dtype = Dtype::NONE;
    REQUIRE_THROWS_WITH(save_state(state, file.path()),
                        ContainsSubstring("dtype is not set"));
    REQUIRE_FALSE(std::filesystem::exists(file.path()));
}

TEST_CASE("read_state_header returns the header without the arrays",
          "[state-file]") {
    TemporaryStateFile file;
    save_state(make_test_state(), file.path());

    const auto header = nlohmann::json::parse(read_state_header(file.path()));
    REQUIRE(header.at("format_version") == 1);
    REQUIRE(header.at("type") == "FastPedestal");
    REQUIRE(header.at("aare_version").is_string());
    REQUIRE(header.at("state").at("dtype") == "<f4");
    REQUIRE(header.at("state").at("n_samples") == 5);
    REQUIRE(header.at("state").at("sum").at("shape") == nlohmann::json{2, 3});

    // The header is readable even when the arrays are not
    const auto bytes = read_bytes(file.path());
    std::filesystem::resize_file(file.path(), state_file_preamble_size +
                                                  header_length(bytes));
    REQUIRE(nlohmann::json::parse(read_state_header(file.path())) == header);
    REQUIRE_THROWS_WITH(load_state<FastPedestalState>(file.path()),
                        ContainsSubstring("extends beyond the end"));
}

TEST_CASE("read_state_header rejects what it cannot read", "[state-file]") {
    TemporaryStateFile file;
    REQUIRE_THROWS_WITH(read_state_header(file.path()),
                        ContainsSubstring("could not open"));

    std::ofstream(file.path(), std::ios::binary) << "not a state file";
    REQUIRE_THROWS_WITH(read_state_header(file.path()),
                        ContainsSubstring("not an aare state file"));

    write_state_file(file.path(), "[1, 2]", "");
    REQUIRE_THROWS_WITH(read_state_header(file.path()),
                        ContainsSubstring("the header must be a JSON object"));

    save_state(make_test_state(), file.path());
    modify_header(file.path(),
                  [](nlohmann::json &header) { header["format_version"] = 2; });
    REQUIRE_THROWS_WITH(read_state_header(file.path()),
                        ContainsSubstring("format version 2 is not supported"));

    // The type and the state are not looked at
    save_state(make_test_state(), file.path());
    modify_header(file.path(), [](nlohmann::json &header) {
        header["type"] = "ClusterFinder";
        header["state"] = 1;
    });
    const auto header = nlohmann::json::parse(read_state_header(file.path()));
    REQUIRE(header.at("type") == "ClusterFinder");
    REQUIRE(header.at("state") == 1);
}

TEMPLATE_TEST_CASE("fast pedestal continues identically from a state file",
                   "[state-file]", double, float, int16_t) {
    auto frame = [](uint32_t frame_index) {
        NDArray<uint16_t, 2> pixels({3, 4});
        for (ssize_t i = 0; i < pixels.size(); ++i) {
            pixels[i] = static_cast<uint16_t>(
                1000 + (frame_index * 37 + static_cast<uint32_t>(i) * 11) % 97);
        }
        return pixels;
    };
    auto push = [&](FastPedestal<TestType> &pedestal, uint32_t frame_index) {
        auto pixels = frame(frame_index);
        if (pedestal.ready()) {
            pedestal.push_ema(pixels.view());
        } else {
            pedestal.add_init_frame(pixels.view());
        }
    };

    // Stored while initializing and after becoming ready
    for (const uint32_t frames_before : {3u, 8u}) {
        CAPTURE(frames_before);
        TemporaryStateFile file;
        FastPedestal<TestType> original(3, 4, 5);
        uint32_t frame_index = 0;
        for (; frame_index < frames_before; ++frame_index) {
            push(original, frame_index);
        }
        save_state(original.state(), file.path());

        const auto header =
            nlohmann::json::parse(read_state_header(file.path()));
        REQUIRE(header.at("state").at("dtype") ==
                pedestal_dtype_string<TestType>());
        const auto state = load_state<FastPedestalState>(file.path());
        REQUIRE(state.dtype == Dtype(typeid(TestType)));

        FastPedestal<TestType> restored(state);
        REQUIRE(restored.ready() == original.ready());

        for (uint32_t i = 0; i < 10; ++i, ++frame_index) {
            push(original, frame_index);
            push(restored, frame_index);
        }
        REQUIRE(same_state(restored.state(), original.state()));
        REQUIRE(restored.mean() == original.mean());
        REQUIRE(restored.std() == original.std());
    }
}

TEST_CASE("load_state ignores header fields it does not know", "[state-file]") {
    TemporaryStateFile file;
    const auto state = make_test_state();
    save_state(state, file.path());

    // The rewritten header is also not padded, so the data is not aligned
    modify_header(file.path(), [](nlohmann::json &header) {
        header["comment"] = "added by a later version";
        header["state"]["later_field"] = {1, 2, 3};
        header["state"]["sum"]["later_field"] = true;
    });

    REQUIRE(same_state(load_state<FastPedestalState>(file.path()), state));
}

TEST_CASE("load_state rejects files that are not state files", "[state-file]") {
    TemporaryStateFile file;
    const auto not_a_state_file = ContainsSubstring("not an aare state file");

    REQUIRE_THROWS_WITH(load_state<FastPedestalState>(file.path()),
                        ContainsSubstring("could not open"));

    std::ofstream(file.path(), std::ios::binary).close();
    REQUIRE_THROWS_WITH(load_state<FastPedestalState>(file.path()),
                        not_a_state_file);

    std::ofstream(file.path(), std::ios::binary) << "AARESTAT";
    REQUIRE_THROWS_WITH(load_state<FastPedestalState>(file.path()),
                        not_a_state_file);

    save_state(make_test_state(), file.path());
    const auto bytes = read_bytes(file.path());
    write_state_file(
        file.path(),
        bytes.substr(state_file_preamble_size, header_length(bytes)),
        bytes.substr(state_file_preamble_size + header_length(bytes)),
        "\x93NUMPY\x01\x00");
    REQUIRE_THROWS_WITH(load_state<FastPedestalState>(file.path()),
                        not_a_state_file);
}

TEST_CASE("load_state rejects a truncated file", "[state-file]") {
    TemporaryStateFile file;
    save_state(make_test_state(), file.path());
    const auto size = std::filesystem::file_size(file.path());

    // One byte of the last array is missing
    std::filesystem::resize_file(file.path(), size - 1);
    REQUIRE_THROWS_WITH(load_state<FastPedestalState>(file.path()),
                        ContainsSubstring("array \"sum2\" extends beyond the "
                                          "end of the file"));

    // Cut in the middle of the header
    std::filesystem::resize_file(file.path(), 40);
    REQUIRE_THROWS_WITH(load_state<FastPedestalState>(file.path()),
                        ContainsSubstring("does not fit in the file"));
}

TEST_CASE("load_state rejects a header that is not JSON", "[state-file]") {
    TemporaryStateFile file;
    write_state_file(file.path(), "{\"format_version\": 1,", "");
    REQUIRE_THROWS_WITH(load_state<FastPedestalState>(file.path()),
                        ContainsSubstring("the header is not valid JSON"));
}

TEST_CASE("load_state rejects headers that do not describe the state",
          "[state-file]") {
    struct Case {
        std::string expected_message;
        std::function<void(nlohmann::json &)> modify;
    };
    using json = nlohmann::json;
    const uint64_t too_large = uint64_t{1} << 62;
    const std::vector<Case> cases{
        {"format version 2 is not supported",
         [](json &h) { h["format_version"] = 2; }},
        {"format version 0 is not supported",
         [](json &h) { h["format_version"] = 0; }},
        {"\"format_version\" is missing",
         [](json &h) { h.erase("format_version"); }},
        {"\"format_version\" must be an integer",
         [](json &h) { h["format_version"] = "1"; }},
        {"holds the state of a ClusterFinder, expected a FastPedestal",
         [](json &h) { h["type"] = "ClusterFinder"; }},
        {"\"type\" must be a string", [](json &h) { h["type"] = 1; }},
        {"\"state\" is missing", [](json &h) { h.erase("state"); }},
        {"\"state\" must be an object",
         [](json &h) { h["state"] = json::array(); }},
        {"\"dtype\" is missing", [](json &h) { h["state"].erase("dtype"); }},
        {"\"dtype\" must be a string",
         [](json &h) { h["state"]["dtype"] = 8; }},
        {"\"dtype\" x8 is not a supported dtype",
         [](json &h) { h["state"]["dtype"] = "x8"; }},
        {"\"dtype\" >f8 is not a supported dtype",
         [](json &h) { h["state"]["dtype"] = ">f8"; }},
        {"\"n_samples\" is missing",
         [](json &h) { h["state"].erase("n_samples"); }},
        {"\"n_samples\" must be an integer between 0 and 4294967295",
         [](json &h) { h["state"]["n_samples"] = -1; }},
        {"\"n_samples\" must be an integer between 0 and 4294967295",
         [](json &h) { h["state"]["n_samples"] = 1.5; }},
        {"\"cur_samples\" must be an integer between 0 and 4294967295",
         [](json &h) { h["state"]["cur_samples"] = uint64_t{1} << 32; }},
        {"\"sum\" is missing", [](json &h) { h["state"].erase("sum"); }},
        {"\"sum\" must describe an array",
         [](json &h) { h["state"]["sum"] = 5; }},
        {"\"sum2\" must describe an array",
         [](json &h) { h["state"]["sum2"].erase("offset"); }},
        {"array \"sum\" has dtype <f4, expected <f8",
         [](json &h) { h["state"]["sum"]["dtype"] = "<f4"; }},
        {"array \"sum\" has 3 dimensions, expected 2",
         [](json &h) { h["state"]["sum"]["shape"] = {2, 3, 1}; }},
        {"array \"sum\" has an invalid shape",
         [](json &h) { h["state"]["sum"]["shape"] = {2, -3}; }},
        {"array \"sum\" has an invalid shape",
         [=](json &h) { h["state"]["sum"]["shape"] = {too_large, too_large}; }},
        {"array \"sum\" extends beyond the end of the file",
         [](json &h) { h["state"]["sum"]["shape"] = {200, 300}; }},
        {"array \"sum2\" extends beyond the end of the file",
         [](json &h) { h["state"]["sum2"]["offset"] = 1000; }},
    };

    for (const auto &test : cases) {
        CAPTURE(test.expected_message);
        TemporaryStateFile file;
        save_state(make_test_state(), file.path());
        modify_header(file.path(), test.modify);
        REQUIRE_THROWS_WITH(load_state<FastPedestalState>(file.path()),
                            ContainsSubstring(test.expected_message));
    }
}
