// SPDX-License-Identifier: MPL-2.0
#include "aare/StateFile.hpp"

#include "aare/Dtype.hpp"
#include "aare/NDArray.hpp"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

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
// holds the state itself under "state": scalars as JSON values, arrays as an
// object {"dtype", "shape", "offset"} with a NumPy dtype string and the
// offset of the data relative to the end of the header. The data section and
// every array in it start on a multiple of 64 bytes.
//
// To support another state type, add a to_header()/from_header() pair for
// it, which can call those of the states it contains, and a save_state()
// overload and load_state() specialization with their declarations in
// StateFile.hpp.

namespace aare {

namespace {

// Insertion order keeps the header readable: identification first, then the
// fields of the state in the order they are written
using json = nlohmann::ordered_json;

constexpr std::array<char, 8> file_magic{'A', 'A', 'R', 'E',
                                         'S', 'T', 'A', 'T'};
constexpr uint64_t format_version = 1;
constexpr uint64_t preamble_size = file_magic.size() + sizeof(uint64_t);
// As in .npy files, so that the arrays can be memory mapped
constexpr uint64_t alignment = 64;

constexpr uint64_t align_up(uint64_t offset) {
    return (offset + alignment - 1) / alignment * alignment;
}

[[noreturn]] void fail(const std::filesystem::path &fname,
                       const std::string &what) {
    throw std::runtime_error(
        fmt::format("State file {}: {}", fname.string(), what));
}

void require_little_endian(const std::filesystem::path &fname) {
    if (endian::native != endian::little)
        fail(fname, "state files are only supported on little-endian systems");
}

// NumPy dtype string of T as it is stored in a state file, for example "<f8"
template <typename T> std::string dtype_string() {
    static_assert(std::is_arithmetic_v<T> && !std::is_same_v<T, bool>,
                  "unsupported array type for a state file");
    const char kind = std::is_floating_point_v<T> ? 'f'
                      : std::is_signed_v<T>       ? 'i'
                                                  : 'u';
    return fmt::format("{}{}{}", sizeof(T) == 1 ? '|' : '<', kind, sizeof(T));
}

// The same for a Dtype known at run time. Dtype::to_string() leaves out the
// byte order of the floating point types
std::string dtype_string(Dtype dtype) {
    std::string text = dtype.to_string();
    if (text.front() != '<')
        text.insert(text.begin(), '<');
    if (dtype.bytes() == 1)
        text.front() = '|';
    return text;
}

/**
 * Collects the arrays of a state while its header is built, then writes the
 * file. The arrays are not copied and must stay alive until write() returns.
 */
class StateWriter {
    struct Array {
        const char *data;
        uint64_t n_bytes;
        uint64_t offset; // relative to the start of the data section
    };
    std::vector<Array> m_arrays;
    uint64_t m_data_size{0};

  public:
    /** Schedule an array for writing and return its description for the
     * header. */
    template <typename T, ssize_t Ndim>
    json add(const NDArray<T, Ndim> &array) {
        const uint64_t offset = align_up(m_data_size);
        const uint64_t n_bytes = array.total_bytes();
        m_arrays.push_back(
            {reinterpret_cast<const char *>(array.data()), n_bytes, offset});
        m_data_size = offset + n_bytes;

        json description;
        description["dtype"] = dtype_string<T>();
        description["shape"] = array.shape();
        description["offset"] = offset;
        return description;
    }

    void write(const std::filesystem::path &fname, std::string_view type,
               json state) const {
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

        auto tmp_name = fname;
        tmp_name += ".tmp";
        try {
            std::ofstream out(tmp_name, std::ios::binary | std::ios::trunc);
            if (!out)
                fail(fname, fmt::format("could not open {} for writing",
                                        tmp_name.string()));
            out.write(file_magic.data(), file_magic.size());
            out.write(reinterpret_cast<const char *>(&header_length),
                      sizeof(header_length));
            out.write(text.data(), static_cast<std::streamsize>(text.size()));

            uint64_t position = 0; // in the data section
            for (const auto &array : m_arrays) {
                for (; position < array.offset; ++position)
                    out.put('\0');
                out.write(array.data,
                          static_cast<std::streamsize>(array.n_bytes));
                position += array.n_bytes;
            }
            out.close();
            if (!out)
                fail(fname,
                     fmt::format("could not write to {}", tmp_name.string()));
            std::filesystem::rename(tmp_name, fname);
        } catch (...) {
            std::error_code ignored;
            std::filesystem::remove(tmp_name, ignored);
            throw;
        }
    }
};

/**
 * Opens a state file, checks that it is one in a format this version can
 * read and, given the expected type of state, hands out the header fields
 * and arrays of the state. Everything read from the file is checked, since
 * the file may be truncated, damaged or not a state file at all.
 */
class StateReader {
    std::filesystem::path m_fname;
    std::ifstream m_file;
    uint64_t m_data_start{};
    uint64_t m_data_size{};
    json m_header;
    json m_state;

    [[noreturn]] void fail(const std::string &what) const {
        aare::fail(m_fname, what);
    }

    const json &field(const json &object, const char *key) const {
        const auto it = object.find(key);
        if (it == object.end())
            fail(fmt::format("\"{}\" is missing", key));
        return *it;
    }

    std::string read_string(const json &object, const char *key) const {
        const json &value = field(object, key);
        if (!value.is_string())
            fail(fmt::format("\"{}\" must be a string", key));
        return value.get<std::string>();
    }

  public:
    /** Open the file and read its header without looking at the state. */
    explicit StateReader(const std::filesystem::path &fname)
        : m_fname(fname), m_file(fname, std::ios::binary) {
        require_little_endian(fname);
        if (!m_file)
            fail("could not open the file");
        std::error_code error;
        const uint64_t file_size = std::filesystem::file_size(fname, error);
        if (error)
            fail(fmt::format("could not get the file size: {}",
                             error.message()));

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
            read_unsigned<uint64_t>(m_header, "format_version");
        if (version == 0 || version > format_version)
            fail(fmt::format("format version {} is not supported, this "
                             "version of aare reads up to version {}",
                             version, format_version));

        m_data_start = preamble_size + header_length;
        m_data_size = file_size - m_data_start;
    }

    /** Open the file and check that it holds the expected type of state. */
    StateReader(const std::filesystem::path &fname,
                std::string_view expected_type)
        : StateReader(fname) {
        const auto type = read_string(m_header, "type");
        if (type != expected_type)
            fail(fmt::format("holds the state of a {}, expected a {}", type,
                             expected_type));
        m_state = field(m_header, "state");
        if (!m_state.is_object())
            fail("\"state\" must be an object");
    }

    /** The complete header. */
    const json &header() const { return m_header; }

    /** The "state" object of the header. */
    const json &state() const { return m_state; }

    /** Read object[key] as a NumPy dtype string with native byte order. */
    Dtype read_dtype(const json &object, const char *key) const {
        const auto text = read_string(object, key);
        try {
            return Dtype(text);
        } catch (const std::runtime_error &) {
            fail(fmt::format("\"{}\" {} is not a supported dtype", key, text));
        }
    }

    /** Read object[key] as an unsigned integer that fits in T. */
    template <typename T>
    T read_unsigned(const json &object, const char *key) const {
        const json &value = field(object, key);
        if (!value.is_number_unsigned() ||
            value.get<uint64_t>() > std::numeric_limits<T>::max())
            fail(fmt::format("\"{}\" must be an integer between 0 and {}", key,
                             std::numeric_limits<T>::max()));
        return static_cast<T>(value.get<uint64_t>());
    }

    /** Read the array that object[key] describes. */
    template <typename T, ssize_t Ndim>
    NDArray<T, Ndim> read_array(const json &object, const char *key) {
        const json &description = field(object, key);
        const auto dtype = description.find("dtype");
        const auto shape = description.find("shape");
        const auto offset = description.find("offset");
        const auto end = description.end();
        if (dtype == end || !dtype->is_string() || shape == end ||
            !shape->is_array() || offset == end ||
            !offset->is_number_unsigned())
            fail(fmt::format("\"{}\" must describe an array with a dtype "
                             "string, a shape list and an offset",
                             key));

        if (dtype->get<std::string>() != dtype_string<T>())
            fail(fmt::format("array \"{}\" has dtype {}, expected {}", key,
                             dtype->get<std::string>(), dtype_string<T>()));
        if (shape->size() != static_cast<size_t>(Ndim))
            fail(fmt::format("array \"{}\" has {} dimensions, expected {}", key,
                             shape->size(), Ndim));

        // The size is checked against the data that is in the file before
        // anything is allocated
        constexpr uint64_t max_extent = std::numeric_limits<ssize_t>::max();
        constexpr uint64_t max_bytes = std::numeric_limits<uint64_t>::max();
        Shape<Ndim> array_shape{};
        uint64_t n_bytes = sizeof(T);
        for (size_t i = 0; i < static_cast<size_t>(Ndim); ++i) {
            const json &extent = (*shape)[i];
            if (!extent.is_number_unsigned() ||
                extent.get<uint64_t>() > max_extent)
                fail(fmt::format("array \"{}\" has an invalid shape", key));
            const auto n = extent.get<uint64_t>();
            if (n != 0 && n_bytes > max_bytes / n)
                fail(fmt::format("array \"{}\" has an invalid shape", key));
            n_bytes *= n;
            array_shape[i] = static_cast<ssize_t>(n);
        }
        const auto data_offset = offset->get<uint64_t>();
        if (data_offset > m_data_size || n_bytes > m_data_size - data_offset)
            fail(fmt::format("array \"{}\" extends beyond the end of the "
                             "file; the file is truncated or damaged",
                             key));

        NDArray<T, Ndim> array(array_shape);
        m_file.seekg(static_cast<std::streamoff>(m_data_start + data_offset));
        m_file.read(reinterpret_cast<char *>(array.data()),
                    static_cast<std::streamsize>(n_bytes));
        if (!m_file)
            fail(fmt::format("could not read array \"{}\"", key));
        return array;
    }
};

constexpr std::string_view fast_pedestal_type = "FastPedestal";

json to_header(const FastPedestalState &state, StateWriter &writer) {
    if (state.dtype == Dtype::NONE || state.dtype == Dtype::ERROR)
        throw std::runtime_error("Cannot save a FastPedestal state whose "
                                 "dtype is not set");
    json header;
    header["dtype"] = dtype_string(state.dtype);
    header["n_samples"] = state.n_samples;
    header["cur_samples"] = state.cur_samples;
    header["sum"] = writer.add(state.sum);
    header["sum2"] = writer.add(state.sum2);
    return header;
}

void from_header(const json &header, StateReader &reader,
                 FastPedestalState &state) {
    state.dtype = reader.read_dtype(header, "dtype");
    state.n_samples = reader.read_unsigned<uint32_t>(header, "n_samples");
    state.cur_samples = reader.read_unsigned<uint32_t>(header, "cur_samples");
    state.sum = reader.read_array<double, 2>(header, "sum");
    state.sum2 = reader.read_array<double, 2>(header, "sum2");
}

} // namespace

std::string read_state_header(const std::filesystem::path &fname) {
    return StateReader(fname).header().dump(2);
}

void save_state(const FastPedestalState &state,
                const std::filesystem::path &fname) {
    StateWriter writer;
    json header = to_header(state, writer);
    writer.write(fname, fast_pedestal_type, std::move(header));
}

template <>
FastPedestalState
load_state<FastPedestalState>(const std::filesystem::path &fname) {
    StateReader reader(fname, fast_pedestal_type);
    FastPedestalState state;
    from_header(reader.state(), reader, state);
    return state;
}

} // namespace aare
