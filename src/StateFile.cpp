// SPDX-License-Identifier: MPL-2.0
#include "aare/StateFile.hpp"

#include "StateFileIO.hpp"

#include "aare/ClusterFinder.hpp"
#include "aare/ClusterFinderMT.hpp"
#include "aare/FastPedestal.hpp"

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

    // Written to a file next to the target, so that the rename stays on one
    // file system, which then replaces the target or is removed again. Saves
    // to one target must not overlap.
    auto tmp = fname;
    tmp += ".tmp";
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out)
        fail(fname, fmt::format("could not create {}", tmp.string()));
    try {
        out.write(file_magic.data(), file_magic.size());
        out.write(reinterpret_cast<const char *>(&header_length),
                  sizeof(header_length));
        out << text;

        uint64_t position = 0; // in the data section
        for (const auto &array : m_arrays) {
            out << std::string(array.offset - position, '\0');
            out.write(array.data, static_cast<std::streamsize>(array.n_bytes));
            position = array.offset + array.n_bytes;
        }
        out.close();
        if (!out)
            fail(fname, fmt::format("could not write to {}", tmp.string()));
        std::filesystem::rename(tmp, fname);
    } catch (...) {
        out.close();
        std::error_code ignored;
        std::filesystem::remove(tmp, ignored);
        throw;
    }
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
    uint64_t version{};
    Fields{*this, m_header}("format_version", version);
    if (version == 0 || version > format_version)
        fail(fmt::format("format version {} is not supported, this "
                         "version of aare reads up to version {}",
                         version, format_version));

    m_data_start = preamble_size + header_length;
    m_data_size = file_size - m_data_start;
}

// A new type of state adds its StateFormat here and its name to the list at
// the end of this file. A state inside a state is a field like any other. A
// field added later gets a value for the files without it, so that older
// files still load; the version increases only for a change that an older
// reader would get wrong.

template <> struct StateFormat<FastPedestalState> {
    static constexpr std::string_view name = "FastPedestal";
    static constexpr uint64_t version = 1;

    template <typename Fields, typename State>
    static void fields(Fields &field, State &state) {
        field("dtype", state.dtype);
        field("n_samples", state.n_samples);
        field("cur_samples", state.cur_samples);
        field("sum", state.sum);
        field("sum2", state.sum2);
    }
};

template <> struct StateFormat<ClusterFinderState> {
    static constexpr std::string_view name = "ClusterFinder";
    static constexpr uint64_t version = 1;

    template <typename Fields, typename State>
    static void fields(Fields &field, State &state) {
        field("cluster_dtype", state.cluster_dtype);
        field("cluster_size_x", state.cluster_size_x);
        field("cluster_size_y", state.cluster_size_y);
        field("n_sigma", state.n_sigma);
        field("threshold", state.threshold);
        field("pedestal", state.pedestal);
    }
};

template <> struct StateFormat<ClusterFinderMTState> {
    static constexpr std::string_view name = "ClusterFinderMT";
    static constexpr uint64_t version = 1;

    template <typename Fields, typename State>
    static void fields(Fields &field, State &state) {
        field("next_thread", state.next_thread);
        field("finders", state.finders);
    }
};

} // namespace detail

std::string read_state_header(const std::filesystem::path &fname) {
    return detail::StateReader(fname).header().dump(2);
}

template <typename State>
void save_state(const State &state, const std::filesystem::path &fname) {
    detail::StateWriter::save(state, fname);
}

template <typename State> State load_state(const std::filesystem::path &fname) {
    return detail::StateReader(fname).state<State>();
}

// The types of state that can be saved and loaded
#define AARE_STATE_FILE_OF(State)                                              \
    template void save_state(const State &, const std::filesystem::path &);    \
    template State load_state<State>(const std::filesystem::path &);
AARE_STATE_FILE_OF(FastPedestalState)
AARE_STATE_FILE_OF(ClusterFinderState)
AARE_STATE_FILE_OF(ClusterFinderMTState)
#undef AARE_STATE_FILE_OF

} // namespace aare
