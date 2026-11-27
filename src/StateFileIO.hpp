// SPDX-License-Identifier: MPL-2.0
#pragma once

// The reader and writer of state files that src/StateFile.cpp builds the
// public functions on. Not installed: nlohmann JSON stays out of the public
// headers. docs/src/StateFile.rst describes the format.

#include "aare/Dtype.hpp"
#include "aare/NDArray.hpp"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <vector>

namespace aare::detail {

// Insertion order keeps the header readable: identification first, then the
// fields of the state in the order they are written
using json = nlohmann::ordered_json;

inline constexpr std::array<char, 8> file_magic{'A', 'A', 'R', 'E',
                                                'S', 'T', 'A', 'T'};
inline constexpr uint64_t format_version = 1;
inline constexpr uint64_t preamble_size = file_magic.size() + sizeof(uint64_t);
// As in .npy files, so that the arrays can be memory mapped
inline constexpr uint64_t alignment = 64;

constexpr uint64_t align_up(uint64_t offset) {
    return (offset + alignment - 1) / alignment * alignment;
}

[[noreturn]] void fail(const std::filesystem::path &fname,
                       const std::string &what);

// NumPy dtype string of T as it is stored in a state file, for example "<f8"
template <typename T> std::string dtype_string() {
    static_assert(std::is_arithmetic_v<T>,
                  "unsupported array type for a state file");
    if constexpr (std::is_same_v<T, bool>)
        return "|b1";
    else
        return Dtype(typeid(T)).to_string();
}

/**
 * How a state is stored in the header, specialized for each type of state
 * with its name ("type" in the file), the version of its fields and
 *
 *   template <typename Fields, typename State>
 *   static void fields(Fields &field, State &state) {
 *       field("key", state.member); // once for each member
 *   }
 *
 * which is called both to write and to read. What a field is in the file
 * follows from the type of the member: a Dtype, an unsigned integer, a
 * double, a bool, an NDArray, another state, a std::vector of states, or a
 * std::optional of one of these for a field that may be left out. A field
 * added later has a third argument, its value when read from an older file.
 */
template <typename State> struct StateFormat;

/**
 * Writes a state to a file: collects the arrays of the state while its
 * header is built, then writes the header and the arrays.
 */
class StateWriter {
    struct Array {
        const char *data;
        uint64_t n_bytes;
        uint64_t offset; // relative to the start of the data section
    };
    std::vector<Array> m_arrays;
    uint64_t m_data_size{0};

    // Writes the fields of one state to its header object
    struct Fields {
        StateWriter &writer;
        std::string_view type;
        json object;

        [[noreturn]] void reject(const char *key, const std::string &is) const {
            throw std::runtime_error(fmt::format(
                "Cannot save a {} state whose {} is {}", type, key, is));
        }

        void operator()(const char *key, const Dtype &value) {
            if (value == Dtype::NONE || value == Dtype::ERROR)
                reject(key, "not set");
            object[key] = value.to_string();
        }

        template <typename T>
        std::enable_if_t<std::is_unsigned_v<T> && !std::is_same_v<T, bool>>
        operator()(const char *key, T value) {
            object[key] = value;
        }

        void operator()(const char *key, bool value) { object[key] = value; }

        void operator()(const char *key, double value) {
            // JSON has no representation for infinity and NaN
            if (!std::isfinite(value))
                reject(key, fmt::to_string(value));
            object[key] = value;
        }

        // The array is scheduled for writing and described in the header
        template <typename T, ssize_t Ndim>
        void operator()(const char *key, const NDArray<T, Ndim> &array) {
            const uint64_t offset = align_up(writer.m_data_size);
            const uint64_t n_bytes = array.total_bytes();
            writer.m_arrays.push_back(
                {reinterpret_cast<const char *>(array.data()), n_bytes,
                 offset});
            writer.m_data_size = offset + n_bytes;

            json &description = object[key];
            description["dtype"] = dtype_string<T>();
            description["shape"] = array.shape();
            description["offset"] = offset;
        }

        template <typename State>
        auto operator()(const char *key, const State &state)
            -> decltype(StateFormat<State>::version, void()) {
            object[key] = writer.object(state);
        }

        template <typename State>
        void operator()(const char *key, const std::vector<State> &states) {
            json &list = object[key] = json::array();
            for (const auto &state : states)
                list.push_back(writer.object(state));
        }

        template <typename T>
        void operator()(const char *key, const std::optional<T> &value) {
            if (value)
                (*this)(key, *value);
        }

        template <typename T>
        void operator()(const char *key, const T &value, const T &) {
            (*this)(key, value);
        }
    };

    // The header object of a state, whose arrays are scheduled for writing
    template <typename State> json object(const State &state) {
        Fields fields{*this, StateFormat<State>::name, {}};
        fields.object["version"] = StateFormat<State>::version;
        StateFormat<State>::fields(fields, state);
        return std::move(fields.object);
    }

    // Write the file: the header with the state in it, then the arrays
    void write(const std::filesystem::path &fname, std::string_view type,
               json state) const;

  public:
    template <typename State>
    static void save(const State &state, const std::filesystem::path &fname) {
        StateWriter writer;
        json header = writer.object(state);
        writer.write(fname, StateFormat<State>::name, std::move(header));
    }
};

/**
 * Opens a state file and checks that it is one in a format this version can
 * read. Everything read from the file is checked, since the file may be
 * truncated, damaged or not a state file at all.
 */
class StateReader {
    std::filesystem::path m_fname;
    std::ifstream m_file;
    uint64_t m_data_start{};
    uint64_t m_data_size{};
    json m_header;

    [[noreturn]] void fail(const std::string &what) const {
        detail::fail(m_fname, what);
    }

    // Reads the fields of one state from its header object
    struct Fields {
        StateReader &reader;
        const json &object;

        [[noreturn]] void fail(const std::string &what) const {
            reader.fail(what);
        }

        const json &at(const char *key) const {
            const auto it = object.find(key);
            if (it == object.end())
                fail(fmt::format("\"{}\" is missing", key));
            return *it;
        }

        // The field, which must be of the kind that is_kind tests for
        const json &at(const char *key, bool (json::*is_kind)() const,
                       std::string_view kind) const {
            const json &field = at(key);
            if (!(field.*is_kind)())
                fail(fmt::format("\"{}\" must be {}", key, kind));
            return field;
        }

        void operator()(const char *key, std::string &value) const {
            value = at(key, &json::is_string, "a string").get<std::string>();
        }

        // A NumPy dtype string with native byte order
        void operator()(const char *key, Dtype &value) const {
            std::string text;
            (*this)(key, text);
            try {
                value = Dtype(text);
            } catch (const std::runtime_error &) {
                fail(fmt::format("\"{}\" {} is not a supported dtype", key,
                                 text));
            }
        }

        template <typename T>
        std::enable_if_t<std::is_unsigned_v<T> && !std::is_same_v<T, bool>>
        operator()(const char *key, T &value) const {
            const json &field = at(key);
            if (!field.is_number_unsigned() ||
                field.get<uint64_t>() > std::numeric_limits<T>::max())
                fail(fmt::format("\"{}\" must be an integer between 0 and {}",
                                 key, std::numeric_limits<T>::max()));
            value = static_cast<T>(field.get<uint64_t>());
        }

        void operator()(const char *key, bool &value) const {
            value = at(key, &json::is_boolean, "true or false").get<bool>();
        }

        void operator()(const char *key, double &value) const {
            value = at(key, &json::is_number, "a number").get<double>();
        }

        template <typename State>
        auto operator()(const char *key, State &state) const
            -> decltype(StateFormat<State>::version, void()) {
            reader.read(at(key, &json::is_object, "an object"), state);
        }

        template <typename State>
        void operator()(const char *key, std::vector<State> &states) const {
            states.clear();
            for (const json &item : at(key, &json::is_array, "a list")) {
                if (!item.is_object())
                    fail(fmt::format("\"{}\" must be a list of objects", key));
                reader.read(item, states.emplace_back());
            }
        }

        // A field that is null counts as left out
        bool has(const char *key) const {
            const auto it = object.find(key);
            return it != object.end() && !it->is_null();
        }

        template <typename T>
        void operator()(const char *key, std::optional<T> &value) const {
            if (has(key))
                (*this)(key, value.emplace());
            else
                value.reset();
        }

        // A field added after the first files were written
        template <typename T>
        void operator()(const char *key, T &value, const T &if_left_out) const {
            if (has(key))
                (*this)(key, value);
            else
                value = if_left_out;
        }

        // The array that the field describes, read from the file
        template <typename T, ssize_t Ndim>
        void operator()(const char *key, NDArray<T, Ndim> &array) const {
            const json &description = at(key);
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
                fail(fmt::format("array \"{}\" has {} dimensions, expected {}",
                                 key, shape->size(), Ndim));

            // The size is checked against the data that is in the file
            // before anything is allocated
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
            if (data_offset > reader.m_data_size ||
                n_bytes > reader.m_data_size - data_offset)
                fail(fmt::format("array \"{}\" extends beyond the end of the "
                                 "file; the file is truncated or damaged",
                                 key));

            array = NDArray<T, Ndim>(array_shape);
            auto &file = reader.m_file;
            file.seekg(
                static_cast<std::streamoff>(reader.m_data_start + data_offset));
            file.read(reinterpret_cast<char *>(array.data()),
                      static_cast<std::streamsize>(n_bytes));
            if (!file)
                fail(fmt::format("could not read array \"{}\"", key));
            // Any other byte is not a value a bool can hold
            if constexpr (std::is_same_v<T, bool>) {
                const auto *bytes =
                    reinterpret_cast<const unsigned char *>(array.data());
                for (uint64_t i = 0; i < n_bytes; ++i)
                    if (bytes[i] > 1)
                        fail(fmt::format("array \"{}\" holds a value that "
                                         "is not 0 or 1",
                                         key));
            }
        }
    };

    // Fields are added to a state without a new version, a change that an
    // older reader would get wrong increases it
    template <typename State> void read(const json &object, State &state) {
        const Fields fields{*this, object};
        uint64_t version{};
        fields("version", version);
        if (version == 0 || version > StateFormat<State>::version)
            fail(fmt::format("version {} of the {} state is not supported, "
                             "this version of aare reads up to version {}",
                             version, StateFormat<State>::name,
                             StateFormat<State>::version));
        StateFormat<State>::fields(fields, state);
    }

  public:
    /** Open the file and read its header without looking at the state. */
    explicit StateReader(const std::filesystem::path &fname);

    /** The complete header. */
    const json &header() const { return m_header; }

    /** The state, after checking that the file holds this type of state. */
    template <typename State> State state() {
        const Fields header{*this, m_header};
        std::string type;
        header("type", type);
        if (type != StateFormat<State>::name)
            fail(fmt::format("holds the state of a {}, expected a {}", type,
                             StateFormat<State>::name));
        State state;
        header("state", state);
        return state;
    }
};

} // namespace aare::detail
