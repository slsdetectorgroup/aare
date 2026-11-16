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
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
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

    /** Write the file: the header with the state in it, then the arrays. */
    void write(const std::filesystem::path &fname, std::string_view type,
               json state) const;
};

/**
 * Opens a state file and checks that it is one in a format this version can
 * read. The state is then read through a Node, starting from state().
 * Everything read from the file is checked, since the file may be truncated,
 * damaged or not a state file at all.
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

  public:
    /** A JSON object of the header with checked access to its fields. */
    class Node {
        StateReader &m_reader;
        const json &m_object;

        [[noreturn]] void fail(const std::string &what) const {
            m_reader.fail(what);
        }

        const json &field(const char *key) const {
            const auto it = m_object.find(key);
            if (it == m_object.end())
                fail(fmt::format("\"{}\" is missing", key));
            return *it;
        }

      public:
        Node(StateReader &reader, const json &object)
            : m_reader(reader), m_object(object) {}

        std::string string(const char *key) const {
            const json &value = field(key);
            if (!value.is_string())
                fail(fmt::format("\"{}\" must be a string", key));
            return value.get<std::string>();
        }

        /** A NumPy dtype string with native byte order. */
        Dtype dtype(const char *key) const {
            const auto text = string(key);
            try {
                return Dtype(text);
            } catch (const std::runtime_error &) {
                fail(fmt::format("\"{}\" {} is not a supported dtype", key,
                                 text));
            }
        }

        /** An unsigned integer that fits in T. */
        template <typename T> T unsigned_integer(const char *key) const {
            const json &value = field(key);
            if (!value.is_number_unsigned() ||
                value.get<uint64_t>() > std::numeric_limits<T>::max())
                fail(fmt::format("\"{}\" must be an integer between 0 and {}",
                                 key, std::numeric_limits<T>::max()));
            return static_cast<T>(value.get<uint64_t>());
        }

        double number(const char *key) const {
            const json &value = field(key);
            if (!value.is_number())
                fail(fmt::format("\"{}\" must be a number", key));
            return value.get<double>();
        }

        /** Whether the object has the field, for one that a state may
         * leave out or that was added after the first files were written.
         * A field that is null counts as left out. */
        bool has(const char *key) const {
            const auto it = m_object.find(key);
            return it != m_object.end() && !it->is_null();
        }

        /** Check the "version" of this state: fields are added without a
         * new version, a change that an older reader would get wrong
         * increases it. */
        void check_version(std::string_view type, uint64_t supported) const {
            const auto version = unsigned_integer<uint64_t>("version");
            if (version == 0 || version > supported)
                fail(fmt::format("version {} of the {} state is not "
                                 "supported, this version of aare reads up "
                                 "to version {}",
                                 version, type, supported));
        }

        /** An object, for a state inside the state. */
        Node child(const char *key) const {
            const json &value = field(key);
            if (!value.is_object())
                fail(fmt::format("\"{}\" must be an object", key));
            return {m_reader, value};
        }

        /** A list of objects, for several states inside the state. */
        std::vector<Node> children(const char *key) const {
            const json &value = field(key);
            if (!value.is_array())
                fail(fmt::format("\"{}\" must be a list", key));
            std::vector<Node> nodes;
            for (const json &item : value) {
                if (!item.is_object())
                    fail(fmt::format("\"{}\" must be a list of objects", key));
                nodes.emplace_back(m_reader, item);
            }
            return nodes;
        }

        /** The array that the field describes, read from the file. */
        template <typename T, ssize_t Ndim>
        NDArray<T, Ndim> array(const char *key) const {
            const json &description = field(key);
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
            if (data_offset > m_reader.m_data_size ||
                n_bytes > m_reader.m_data_size - data_offset)
                fail(fmt::format("array \"{}\" extends beyond the end of the "
                                 "file; the file is truncated or damaged",
                                 key));

            NDArray<T, Ndim> array(array_shape);
            auto &file = m_reader.m_file;
            file.seekg(static_cast<std::streamoff>(m_reader.m_data_start +
                                                   data_offset));
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
            return array;
        }
    };

    /** Open the file and read its header without looking at the state. */
    explicit StateReader(const std::filesystem::path &fname);

    /** The complete header. */
    const json &header() const { return m_header; }

    /** The "state" object, after checking that the file holds the expected
     * type of state. */
    Node state(std::string_view expected_type);
};

} // namespace aare::detail
