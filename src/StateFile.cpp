// SPDX-License-Identifier: MPL-2.0
#include "aare/StateFile.hpp"

#include "StateFileIO.hpp"

#include "aare/ClusterFinder.hpp"
#include "aare/FastPedestal.hpp"

#include <cmath>
#include <system_error>
#include <utility>

#ifndef AARE_VERSION
#define AARE_VERSION "unknown"
#endif

// Layout of a state file:
//
//   bytes 0-7    magic "AARESTAT"
//   bytes 8-15   length N of the header, uint64 little-endian
//   N bytes      JSON header, padded with spaces
//   rest         arrays as raw little-endian data in C order
//
// The header identifies the file (format_version, aare_version, type) and
// holds the state itself under "state": the version of its fields, scalars
// as JSON values, states inside it as objects or lists of them, arrays as an
// object {"dtype", "shape", "offset"} with a NumPy dtype string and the
// offset of the data relative to the end of the header. The data section and
// every array in it start on a multiple of 64 bytes.

namespace aare {

namespace detail {

void fail(const std::filesystem::path &fname, const std::string &what) {
    throw std::runtime_error(
        fmt::format("State file {}: {}", fname.string(), what));
}

namespace {

void require_little_endian(const std::filesystem::path &fname) {
    if (endian::native != endian::little)
        fail(fname, "state files are only supported on little-endian systems");
}

// The file a state is written to before it replaces the target: the target
// with ".tmp" appended, so that the rename stays on one file system. Removed
// again unless it was renamed. Saves to one target must not overlap.
class TemporaryFile {
    std::filesystem::path m_target;
    std::filesystem::path m_path;
    std::ofstream m_file;

  public:
    explicit TemporaryFile(const std::filesystem::path &target)
        : m_target(target), m_path(target) {
        m_path += ".tmp";
        m_file.open(m_path, std::ios::binary | std::ios::trunc);
        if (!m_file)
            fail(target, fmt::format("could not create {}", m_path.string()));
    }

    TemporaryFile(const TemporaryFile &) = delete;
    TemporaryFile &operator=(const TemporaryFile &) = delete;

    ~TemporaryFile() {
        m_file.close();
        if (!m_path.empty()) {
            std::error_code ignored;
            std::filesystem::remove(m_path, ignored);
        }
    }

    // A failed write is reported by rename_to_target()
    void write(const char *data, uint64_t n_bytes) {
        m_file.write(data, static_cast<std::streamsize>(n_bytes));
    }

    void pad(uint64_t n_bytes) {
        for (uint64_t i = 0; i < n_bytes; ++i)
            m_file.put('\0');
    }

    /** Replace the target with what was written. */
    void rename_to_target() {
        m_file.close();
        if (!m_file)
            fail(m_target,
                 fmt::format("could not write to {}", m_path.string()));
        std::filesystem::rename(m_path, m_target);
        m_path.clear();
    }
};

} // namespace

void StateWriter::write(const std::filesystem::path &fname,
                        std::string_view type, json state) const {
    require_little_endian(fname);

    json header;
    header["format_version"] = format_version;
    header["aare_version"] = AARE_VERSION;
    header["type"] = std::string(type);
    header["state"] = std::move(state);

    // Padded with spaces before the final newline so that the data
    // section starts aligned
    std::string text = header.dump(2);
    const uint64_t unpadded = preamble_size + text.size() + 1;
    text.append(align_up(unpadded) - unpadded, ' ');
    text.push_back('\n');
    const uint64_t header_length = text.size();

    TemporaryFile out(fname);
    out.write(file_magic.data(), file_magic.size());
    out.write(reinterpret_cast<const char *>(&header_length),
              sizeof(header_length));
    out.write(text.data(), text.size());

    uint64_t position = 0; // in the data section
    for (const auto &array : m_arrays) {
        out.pad(array.offset - position);
        out.write(array.data, array.n_bytes);
        position = array.offset + array.n_bytes;
    }
    out.rename_to_target();
}

StateReader::StateReader(const std::filesystem::path &fname)
    : m_fname(fname), m_file(fname, std::ios::binary) {
    require_little_endian(fname);
    if (!m_file)
        fail("could not open the file");
    std::error_code error;
    const uint64_t file_size = std::filesystem::file_size(fname, error);
    if (error)
        fail(fmt::format("could not get the file size: {}", error.message()));

    std::array<char, 8> magic{};
    uint64_t header_length = 0;
    m_file.read(magic.data(), magic.size());
    m_file.read(reinterpret_cast<char *>(&header_length),
                sizeof(header_length));
    if (!m_file || magic != file_magic)
        fail("not an aare state file");
    if (header_length > file_size - preamble_size)
        fail(fmt::format("the header of {} bytes does not fit in the "
                         "file of {} bytes; the file is truncated or "
                         "damaged",
                         header_length, file_size));

    std::string text(header_length, '\0');
    m_file.read(text.data(), static_cast<std::streamsize>(header_length));
    if (!m_file)
        fail("could not read the header");
    try {
        m_header = json::parse(text);
    } catch (const json::exception &e) {
        fail(fmt::format("the header is not valid JSON: {}", e.what()));
    }
    if (!m_header.is_object())
        fail("the header must be a JSON object");

    // The version comes first: a newer format may have changed the rest
    const auto version =
        Node(*this, m_header).unsigned_integer<uint64_t>("format_version");
    if (version == 0 || version > format_version)
        fail(fmt::format("format version {} is not supported, this "
                         "version of aare reads up to version {}",
                         version, format_version));

    m_data_start = preamble_size + header_length;
    m_data_size = file_size - m_data_start;
}

StateReader::Node StateReader::state(std::string_view expected_type) {
    const Node header(*this, m_header);
    const auto type = header.string("type");
    if (type != expected_type)
        fail(fmt::format("holds the state of a {}, expected a {}", type,
                         expected_type));
    return header.child("state");
}

} // namespace detail

namespace {

using detail::json;
using detail::StateReader;
using detail::StateWriter;

// A new type of state adds its name in the file, the version of its fields
// and how it maps to the header and back. A state inside a state calls the
// pair of its own type. A field added later is read only if the header
// has() it, so that older files still load; the version increases only for
// a change that an older reader would get wrong.

constexpr std::string_view type_name(const FastPedestalState &) {
    return "FastPedestal";
}
constexpr uint64_t state_version(const FastPedestalState &) { return 1; }

json to_header(const FastPedestalState &state, StateWriter &writer) {
    if (state.dtype == Dtype::NONE || state.dtype == Dtype::ERROR)
        throw std::runtime_error("Cannot save a FastPedestal state whose "
                                 "dtype is not set");
    json header;
    header["version"] = state_version(state);
    header["dtype"] = state.dtype.to_string();
    header["n_samples"] = state.n_samples;
    header["cur_samples"] = state.cur_samples;
    header["sum"] = writer.add(state.sum);
    header["sum2"] = writer.add(state.sum2);
    return header;
}

void from_header(const StateReader::Node &header, FastPedestalState &state) {
    header.check_version(type_name(state), state_version(state));
    state.dtype = header.dtype("dtype");
    state.n_samples = header.unsigned_integer<uint32_t>("n_samples");
    state.cur_samples = header.unsigned_integer<uint32_t>("cur_samples");
    state.sum = header.array<double, 2>("sum");
    state.sum2 = header.array<double, 2>("sum2");
}

constexpr std::string_view type_name(const ClusterFinderState &) {
    return "ClusterFinder";
}
constexpr uint64_t state_version(const ClusterFinderState &) { return 1; }

json to_header(const ClusterFinderState &state, StateWriter &writer) {
    if (state.cluster_dtype == Dtype::NONE ||
        state.cluster_dtype == Dtype::ERROR)
        throw std::runtime_error("Cannot save a ClusterFinder state whose "
                                 "cluster_dtype is not set");
    // JSON has no representation for them
    if (!std::isfinite(state.n_sigma))
        throw std::runtime_error(
            fmt::format("Cannot save a ClusterFinder state whose n_sigma is {}",
                        state.n_sigma));
    json header;
    header["version"] = state_version(state);
    header["cluster_dtype"] = state.cluster_dtype.to_string();
    header["cluster_size_x"] = state.cluster_size_x;
    header["cluster_size_y"] = state.cluster_size_y;
    header["n_sigma"] = state.n_sigma;
    header["threshold"] = writer.add(state.threshold);
    header["pedestal"] = to_header(state.pedestal, writer);
    return header;
}

void from_header(const StateReader::Node &header, ClusterFinderState &state) {
    header.check_version(type_name(state), state_version(state));
    state.cluster_dtype = header.dtype("cluster_dtype");
    state.cluster_size_x = header.unsigned_integer<uint8_t>("cluster_size_x");
    state.cluster_size_y = header.unsigned_integer<uint8_t>("cluster_size_y");
    state.n_sigma = header.number("n_sigma");
    state.threshold = header.array<double, 2>("threshold");
    from_header(header.child("pedestal"), state.pedestal);
}

template <typename State>
void save(const State &state, const std::filesystem::path &fname) {
    StateWriter writer;
    json header = to_header(state, writer);
    writer.write(fname, type_name(state), std::move(header));
}

template <typename State> State load(const std::filesystem::path &fname) {
    StateReader reader(fname);
    State state;
    from_header(reader.state(type_name(state)), state);
    return state;
}

} // namespace

std::string read_state_header(const std::filesystem::path &fname) {
    return detail::StateReader(fname).header().dump(2);
}

void save_state(const FastPedestalState &state,
                const std::filesystem::path &fname) {
    save(state, fname);
}

template <>
FastPedestalState
load_state<FastPedestalState>(const std::filesystem::path &fname) {
    return load<FastPedestalState>(fname);
}

void save_state(const ClusterFinderState &state,
                const std::filesystem::path &fname) {
    save(state, fname);
}

template <>
ClusterFinderState
load_state<ClusterFinderState>(const std::filesystem::path &fname) {
    return load<ClusterFinderState>(fname);
}

} // namespace aare
