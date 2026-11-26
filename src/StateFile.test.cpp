// SPDX-License-Identifier: MPL-2.0
#include "aare/StateFile.hpp"

#include "StateFileIO.hpp"

#include "aare/ClusterFinder.hpp"
#include "aare/ClusterFinderMT.hpp"
#include "aare/FastPedestal.hpp"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <nlohmann/json.hpp>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <sstream>
#include <vector>

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
        for (const auto &tmp : tmp_files())
            std::filesystem::remove(tmp, error);
    }

    const std::filesystem::path &path() const { return m_path; }

    // The files save_state() wrote to and did not rename or remove
    std::vector<std::filesystem::path> tmp_files() const {
        const auto prefix = m_path.filename().string() + ".";
        std::vector<std::filesystem::path> found;
        for (const auto &entry :
             std::filesystem::directory_iterator(m_path.parent_path()))
            if (entry.path().filename().string().rfind(prefix, 0) == 0)
                found.push_back(entry.path());
        return found;
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

// A finder state around make_test_state(), with the same kind of values
ClusterFinderState make_test_finder_state() {
    ClusterFinderState state;
    state.cluster_dtype = Dtype::INT32;
    state.cluster_size_x = 3;
    state.cluster_size_y = 5;
    state.n_sigma = 0.1;
    state.threshold = NDArray<double, 2>({2, 3});
    for (ssize_t i = 0; i < 6; ++i) {
        state.threshold[i] = static_cast<double>(i + 1) / 7.0;
    }
    state.threshold[4] = std::numeric_limits<double>::quiet_NaN();
    state.pedestal = make_test_state();
    return state;
}

bool same_state(const ClusterFinderState &a, const ClusterFinderState &b) {
    return a.cluster_dtype == b.cluster_dtype &&
           a.cluster_size_x == b.cluster_size_x &&
           a.cluster_size_y == b.cluster_size_y &&
           std::memcmp(&a.n_sigma, &b.n_sigma, sizeof(double)) == 0 &&
           same_bits(a.threshold, b.threshold) &&
           same_state(a.pedestal, b.pedestal);
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
    REQUIRE(file.tmp_files().empty());
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
        {"version 2 of the FastPedestal state is not supported",
         [](json &h) { h["state"]["version"] = 2; }},
        {"\"version\" is missing",
         [](json &h) { h["state"].erase("version"); }},
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

TEST_CASE("state file returns a ClusterFinderState bit for bit",
          "[state-file]") {
    TemporaryStateFile file;
    const auto state = make_test_finder_state();
    save_state(state, file.path());

    const auto loaded = load_state<ClusterFinderState>(file.path());
    REQUIRE(loaded.cluster_dtype == Dtype::INT32);
    REQUIRE(loaded.cluster_size_x == 3);
    REQUIRE(loaded.cluster_size_y == 5);
    REQUIRE(loaded.n_sigma == 0.1);
    REQUIRE(loaded.threshold.shape() == Shape<2>{2, 3});
    REQUIRE(loaded.pedestal.dtype == Dtype::FLOAT);
    REQUIRE(same_state(loaded, state));
}

TEST_CASE("state file of a cluster finder has the documented layout",
          "[state-file]") {
    TemporaryStateFile file;
    const auto state = make_test_finder_state();
    save_state(state, file.path());
    const auto bytes = read_bytes(file.path());

    REQUIRE(bytes.substr(0, 8) == "AARESTAT");
    const auto data_start = state_file_preamble_size + header_length(bytes);
    REQUIRE(data_start % 64 == 0);

    const auto header = nlohmann::json::parse(
        bytes.substr(state_file_preamble_size, header_length(bytes)));
    REQUIRE(header == nlohmann::json::parse(read_state_header(file.path())));
    REQUIRE(header.at("format_version") == 1);
    REQUIRE(header.at("type") == "ClusterFinder");

    const auto &fields = header.at("state");
    REQUIRE(fields.at("cluster_dtype") == "<i4");
    REQUIRE(fields.at("cluster_size_x") == 3);
    REQUIRE(fields.at("cluster_size_y") == 5);
    REQUIRE(fields.at("n_sigma") == 0.1);
    // The pedestal is stored as in its own file, its arrays after the
    // threshold, each on the next multiple of 64 bytes
    const size_t array_bytes = 2 * 3 * sizeof(double);
    const nlohmann::json expected_shape{2, 3};
    const auto &pedestal = fields.at("pedestal");
    REQUIRE(pedestal.at("dtype") == "<f4");
    REQUIRE(pedestal.at("n_samples") == 5);
    REQUIRE(pedestal.at("cur_samples") == 3);
    const std::array<const nlohmann::json *, 3> arrays{
        &fields.at("threshold"), &pedestal.at("sum"), &pedestal.at("sum2")};
    const std::array<const double *, 3> data{state.threshold.data(),
                                             state.pedestal.sum.data(),
                                             state.pedestal.sum2.data()};
    for (size_t i = 0; i < arrays.size(); ++i) {
        CAPTURE(i);
        REQUIRE(arrays[i]->at("dtype") == "<f8");
        REQUIRE(arrays[i]->at("shape") == expected_shape);
        REQUIRE(arrays[i]->at("offset") == 64 * i);
        REQUIRE(std::memcmp(bytes.data() + data_start + 64 * i, data[i],
                            array_bytes) == 0);
    }
    REQUIRE(bytes.size() == data_start + 128 + array_bytes);
}

TEST_CASE("state file of a cluster finder needs what it records",
          "[state-file]") {
    TemporaryStateFile file;

    auto no_cluster_dtype = make_test_finder_state();
    no_cluster_dtype.cluster_dtype = Dtype::NONE;
    REQUIRE_THROWS_WITH(save_state(no_cluster_dtype, file.path()),
                        ContainsSubstring("cluster_dtype is not set"));

    auto no_pedestal_dtype = make_test_finder_state();
    no_pedestal_dtype.pedestal.dtype = Dtype::NONE;
    REQUIRE_THROWS_WITH(save_state(no_pedestal_dtype, file.path()),
                        ContainsSubstring("dtype is not set"));

    for (const double n_sigma : {std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::quiet_NaN()}) {
        auto not_finite = make_test_finder_state();
        not_finite.n_sigma = n_sigma;
        REQUIRE_THROWS_WITH(save_state(not_finite, file.path()),
                            ContainsSubstring("n_sigma is"));
    }
    REQUIRE_FALSE(std::filesystem::exists(file.path()));
}

TEST_CASE("load_state only reads the type of state it is asked for",
          "[state-file]") {
    TemporaryStateFile pedestal_file;
    TemporaryStateFile finder_file;
    save_state(make_test_state(), pedestal_file.path());
    save_state(make_test_finder_state(), finder_file.path());

    REQUIRE_THROWS_WITH(
        load_state<ClusterFinderState>(pedestal_file.path()),
        ContainsSubstring(
            "holds the state of a FastPedestal, expected a ClusterFinder"));
    REQUIRE_THROWS_WITH(
        load_state<FastPedestalState>(finder_file.path()),
        ContainsSubstring(
            "holds the state of a ClusterFinder, expected a FastPedestal"));
}

TEMPLATE_TEST_CASE("cluster finder continues identically from a state file",
                   "[state-file]", double, float) {
    using Finder = ClusterFinder<Cluster<int32_t, 3, 3>, uint16_t, TestType>;
    const Shape<2> shape{10, 12};
    auto process = [&](Finder &finder, uint32_t frame_index) {
        NDArray<uint16_t, 2> pixels(shape);
        for (ssize_t i = 0; i < pixels.size(); ++i) {
            pixels[i] = static_cast<uint16_t>(
                1000 + (frame_index * 37 + static_cast<uint32_t>(i) * 11) % 7);
        }
        // A photon in every frame after those of the pedestal
        if (frame_index >= 5) {
            const ssize_t hit = 2 + frame_index % 5;
            pixels(hit, hit) = static_cast<uint16_t>(pixels(hit, hit) + 300);
        }
        if (finder.pedestal_ready()) {
            finder.find_clusters(pixels.view(), frame_index);
        } else {
            finder.push_pedestal_frame(pixels.view());
        }
    };

    // Stored while the pedestal initializes and after finding clusters
    for (const uint32_t frames_before : {3u, 12u}) {
        CAPTURE(frames_before);
        TemporaryStateFile file;
        // A multiplier that is not exact in float or in decimal
        Finder original(shape, static_cast<TestType>(4.1), 100, 5);
        uint32_t frame_index = 0;
        for (; frame_index < frames_before; ++frame_index) {
            process(original, frame_index);
        }
        original.steal_clusters();
        save_state(original.state(), file.path());

        const auto state = load_state<ClusterFinderState>(file.path());
        REQUIRE(same_state(state, original.state()));

        Finder restored(state);
        REQUIRE(restored.get_nSigma() == original.get_nSigma());
        for (uint32_t i = 0; i < 10; ++i, ++frame_index) {
            process(original, frame_index);
            process(restored, frame_index);
        }
        REQUIRE(same_state(restored.state(), original.state()));

        const auto expected = original.steal_clusters();
        const auto actual = restored.steal_clusters();
        REQUIRE(expected.size() > 0);
        REQUIRE(actual.size() == expected.size());
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(actual[i].x == expected[i].x);
            REQUIRE(actual[i].y == expected[i].y);
            REQUIRE(actual[i].data == expected[i].data);
        }
    }
}

TEST_CASE("load_state rejects headers that do not describe a cluster finder",
          "[state-file]") {
    struct Case {
        std::string expected_message;
        std::function<void(nlohmann::json &)> modify;
    };
    using json = nlohmann::json;
    const std::vector<Case> cases{
        {"\"cluster_dtype\" is missing",
         [](json &h) { h["state"].erase("cluster_dtype"); }},
        {"\"cluster_dtype\" x4 is not a supported dtype",
         [](json &h) { h["state"]["cluster_dtype"] = "x4"; }},
        {"\"cluster_size_x\" is missing",
         [](json &h) { h["state"].erase("cluster_size_x"); }},
        {"\"cluster_size_y\" must be an integer between 0 and 255",
         [](json &h) { h["state"]["cluster_size_y"] = 256; }},
        {"\"n_sigma\" is missing",
         [](json &h) { h["state"].erase("n_sigma"); }},
        {"\"n_sigma\" must be a number",
         [](json &h) { h["state"]["n_sigma"] = "5"; }},
        {"\"n_sigma\" must be a number",
         [](json &h) { h["state"]["n_sigma"] = nullptr; }},
        {"\"threshold\" is missing",
         [](json &h) { h["state"].erase("threshold"); }},
        {"array \"threshold\" has dtype <f4, expected <f8",
         [](json &h) { h["state"]["threshold"]["dtype"] = "<f4"; }},
        {"array \"threshold\" extends beyond the end of the file",
         [](json &h) { h["state"]["threshold"]["shape"] = {200, 300}; }},
        {"\"pedestal\" is missing",
         [](json &h) { h["state"].erase("pedestal"); }},
        {"\"pedestal\" must be an object",
         [](json &h) { h["state"]["pedestal"] = 5; }},
        {"\"n_samples\" is missing",
         [](json &h) { h["state"]["pedestal"].erase("n_samples"); }},
        {"\"sum\" must describe an array",
         [](json &h) { h["state"]["pedestal"]["sum"] = 5; }},
        {"array \"sum2\" extends beyond the end of the file",
         [](json &h) { h["state"]["pedestal"]["sum2"]["offset"] = 1000; }},
    };

    for (const auto &test : cases) {
        CAPTURE(test.expected_message);
        TemporaryStateFile file;
        save_state(make_test_finder_state(), file.path());
        modify_header(file.path(), test.modify);
        REQUIRE_THROWS_WITH(load_state<ClusterFinderState>(file.path()),
                            ContainsSubstring(test.expected_message));
    }
}

TEST_CASE("state inside a state has its own version", "[state-file]") {
    TemporaryStateFile file;
    save_state(make_test_finder_state(), file.path());
    REQUIRE(nlohmann::json::parse(read_state_header(file.path()))
                .at("state")
                .at("version") == 1);
    REQUIRE(nlohmann::json::parse(read_state_header(file.path()))
                .at("state")
                .at("pedestal")
                .at("version") == 1);

    modify_header(file.path(), [](nlohmann::json &header) {
        header["state"]["pedestal"]["version"] = 2;
    });
    REQUIRE_THROWS_WITH(
        load_state<ClusterFinderState>(file.path()),
        ContainsSubstring(
            "version 2 of the FastPedestal state is not supported"));
}

// What the states of the classes that follow need from the reader and
// writer, which no state uses yet: fields that may be left out or were
// added later, and bool arrays
namespace {

struct TestThreadState {
    uint32_t index{};
};

struct TestState {
    NDArray<bool, 2> mask;
    std::optional<NDArray<double, 2>> noise_map;
    std::vector<TestThreadState> threads;
    uint32_t added_later{};
};

} // namespace

namespace aare::detail {

template <> struct StateFormat<TestThreadState> {
    static constexpr std::string_view name = "TestThread";
    static constexpr uint64_t version = 1;

    template <typename Fields, typename State>
    static void fields(Fields &field, State &state) {
        field("index", state.index);
    }
};

template <> struct StateFormat<TestState> {
    static constexpr std::string_view name = "Test";
    static constexpr uint64_t version = 1;

    template <typename Fields, typename State>
    static void fields(Fields &field, State &state) {
        field("mask", state.mask);
        field("noise_map", state.noise_map);
        field("threads", state.threads);
        field("added_later", state.added_later, uint32_t{7});
    }
};

} // namespace aare::detail

TEST_CASE("state reader and writer handle optional fields, lists and masks",
          "[state-file]") {
    using json = nlohmann::json;
    TemporaryStateFile file;
    const auto load = [&file] {
        return detail::StateReader(file.path()).state<TestState>();
    };

    TestState state;
    state.mask = NDArray<bool, 2>(Shape<2>{2, 3}, false);
    state.mask(0, 1) = true;
    state.mask(1, 2) = true;
    state.threads = {{0}, {1}};
    state.added_later = 3;
    detail::StateWriter::save(state, file.path());

    const auto header = json::parse(read_state_header(file.path()));
    REQUIRE(header.at("type") == "Test");
    REQUIRE(header.at("state").at("mask").at("dtype") == "|b1");
    REQUIRE_FALSE(header.at("state").contains("noise_map"));

    auto loaded = load();
    REQUIRE(loaded.mask == state.mask);
    REQUIRE_FALSE(loaded.noise_map.has_value());
    REQUIRE(loaded.threads.size() == 2);
    REQUIRE(loaded.threads[0].index == 0);
    REQUIRE(loaded.threads[1].index == 1);
    REQUIRE(loaded.added_later == 3);

    SECTION("a field that is null counts as left out") {
        modify_header(file.path(),
                      [](json &h) { h["state"]["noise_map"] = nullptr; });
        REQUIRE_FALSE(load().noise_map.has_value());
    }
    SECTION("a field that may be left out is read if it is there") {
        state.noise_map = NDArray<double, 2>(Shape<2>{2, 3}, 0.5);
        detail::StateWriter::save(state, file.path());
        loaded = load();
        REQUIRE(loaded.mask == state.mask);
        REQUIRE(loaded.noise_map.has_value());
        REQUIRE(*loaded.noise_map == *state.noise_map);

        modify_header(file.path(),
                      [](json &h) { h["state"]["noise_map"]["shape"] = {2}; });
        REQUIRE_THROWS_WITH(
            load(), ContainsSubstring("array \"noise_map\" has 1 dimensions"));
    }
    SECTION("a field added later has a value for the files without it") {
        modify_header(file.path(),
                      [](json &h) { h["state"].erase("added_later"); });
        REQUIRE(load().added_later == 7);
        modify_header(file.path(),
                      [](json &h) { h["state"]["added_later"] = -1; });
        REQUIRE_THROWS_WITH(
            load(), ContainsSubstring("\"added_later\" must be an integer"));
    }
    SECTION("a list must be a list of objects") {
        modify_header(file.path(), [](json &h) {
            h["state"]["threads"] = h["state"]["mask"];
        });
        REQUIRE_THROWS_WITH(load(),
                            ContainsSubstring("\"threads\" must be a list"));
        modify_header(file.path(),
                      [](json &h) { h["state"]["threads"] = {1, 2}; });
        REQUIRE_THROWS_WITH(
            load(), ContainsSubstring("\"threads\" must be a list of objects"));
    }
    SECTION("a bool array only holds 0 and 1") {
        auto bytes = read_bytes(file.path());
        const auto data_start = state_file_preamble_size + header_length(bytes);
        REQUIRE(bytes[data_start + 1] == 1);
        bytes[data_start + 1] = 2;
        write_state_file(
            file.path(),
            bytes.substr(state_file_preamble_size, header_length(bytes)),
            bytes.substr(data_start));
        REQUIRE_THROWS_WITH(
            load(), ContainsSubstring("holds a value that is not 0 or 1"));
    }
}

namespace {

// Two cluster finders that differ, as those of two threads do
ClusterFinderMTState make_test_mt_state() {
    ClusterFinderMTState state;
    state.finders.push_back(make_test_finder_state());
    state.finders.push_back(make_test_finder_state());
    state.finders[1].threshold[0] = 5.0;
    state.finders[1].pedestal.sum[1] = 6.0;
    state.next_thread = 1;
    return state;
}

bool same_state(const ClusterFinderMTState &a, const ClusterFinderMTState &b) {
    if (a.next_thread != b.next_thread || a.finders.size() != b.finders.size())
        return false;
    for (size_t i = 0; i < a.finders.size(); ++i) {
        if (!same_state(a.finders[i], b.finders[i]))
            return false;
    }
    return true;
}

} // namespace

TEST_CASE("state file returns a ClusterFinderMTState bit for bit",
          "[state-file]") {
    TemporaryStateFile file;
    const auto state = make_test_mt_state();
    save_state(state, file.path());

    const auto loaded = load_state<ClusterFinderMTState>(file.path());
    REQUIRE(loaded.finders.size() == 2);
    REQUIRE(loaded.next_thread == 1);
    REQUIRE(same_state(loaded, state));
    REQUIRE_FALSE(same_state(loaded.finders[0], loaded.finders[1]));
}

TEST_CASE("state file of a multithreaded cluster finder has the documented "
          "layout",
          "[state-file]") {
    TemporaryStateFile file;
    save_state(make_test_mt_state(), file.path());

    const auto header = nlohmann::json::parse(read_state_header(file.path()));
    REQUIRE(header.at("type") == "ClusterFinderMT");
    const auto &state = header.at("state");
    REQUIRE(state.at("version") == 1);
    REQUIRE(state.at("next_thread") == 1);
    const auto &finders = state.at("finders");
    REQUIRE(finders.is_array());
    REQUIRE(finders.size() == 2);

    // Every array of every thread has its own aligned place in the data
    std::vector<uint64_t> offsets;
    for (const auto &finder : finders) {
        REQUIRE(finder.at("version") == 1);
        REQUIRE(finder.at("cluster_size_y") == 5);
        offsets.push_back(finder.at("threshold").at("offset"));
        offsets.push_back(finder.at("pedestal").at("sum").at("offset"));
        offsets.push_back(finder.at("pedestal").at("sum2").at("offset"));
    }
    REQUIRE(offsets == std::vector<uint64_t>{0, 64, 128, 192, 256, 320});
}

TEST_CASE("state file keeps a multithreaded cluster finder without cluster "
          "finders, which the class rejects",
          "[state-file]") {
    TemporaryStateFile file;
    save_state(ClusterFinderMTState{}, file.path());
    const auto loaded = load_state<ClusterFinderMTState>(file.path());
    REQUIRE(loaded.finders.empty());
    using Finder = ClusterFinderMT<Cluster<int32_t, 3, 5>>;
    REQUIRE_THROWS_AS(Finder{loaded}, std::runtime_error);
}

TEST_CASE("load_state rejects headers that do not describe a multithreaded "
          "cluster finder",
          "[state-file]") {
    struct Case {
        std::string expected_message;
        std::function<void(nlohmann::json &)> modify;
    };
    using json = nlohmann::json;
    const std::vector<Case> cases{
        {"version 2 of the ClusterFinderMT state is not supported",
         [](json &h) { h["state"]["version"] = 2; }},
        {"\"next_thread\" is missing",
         [](json &h) { h["state"].erase("next_thread"); }},
        {"\"finders\" is missing",
         [](json &h) { h["state"].erase("finders"); }},
        {"\"finders\" must be a list",
         [](json &h) { h["state"]["finders"] = h["state"]["finders"][0]; }},
        {"\"finders\" must be a list of objects",
         [](json &h) { h["state"]["finders"][1] = 1; }},
        {"version 2 of the ClusterFinder state is not supported",
         [](json &h) { h["state"]["finders"][1]["version"] = 2; }},
        {"\"n_sigma\" is missing",
         [](json &h) { h["state"]["finders"][1].erase("n_sigma"); }},
    };
    for (const auto &test_case : cases) {
        TemporaryStateFile file;
        save_state(make_test_mt_state(), file.path());
        modify_header(file.path(), test_case.modify);
        REQUIRE_THROWS_WITH(load_state<ClusterFinderMTState>(file.path()),
                            ContainsSubstring(test_case.expected_message));
    }

    TemporaryStateFile file;
    save_state(make_test_mt_state(), file.path());
    REQUIRE_THROWS_WITH(
        load_state<ClusterFinderState>(file.path()),
        ContainsSubstring(
            "holds the state of a ClusterFinderMT, expected a ClusterFinder"));
}
