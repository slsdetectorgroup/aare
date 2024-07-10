#include <filesystem>
#include <string>
#include <vector>

#include "simdjson.h"

#include "aare/core/Dtype.hpp"
#include "aare/core/defs.hpp"
namespace aare {
/**
 * in Binary format:
 *    - magic_string: 4 bytes ("CLST")
 *    - header_length: 4 bytes (uint32)
 *
 * in JSON format:
 *     - version: "0.1" (3 char string)
 *     - n_records (doesn't exceed 2^64 unsigned)
 *     - metadata: metadata (json string max 2^16 chars. no nested json objects, only key-value pairs of strings)
 *     - header_fields (array of field objects):
 *         - field_label: field_label_length bytes (string max 256 chars)
 *         - dtype: 3 chars (string)
 *         - is_array: (number) 0: not array, 1:fixed_length_array, 2:variable_length_array
 *         - array_length: (number max 2^32) (used if is_array == 1)
 *
 *         if field is:
 *            - not array: value of the field (dtype.bytes())
 *            - fixed_length_array: array_length * dtype.bytes()
 *            - variable_length_array: 4 bytes (number of elements) + number_of_elements * dtype.bytes()
 *
 *     - header_fields (array of field objects):
 *         - field_label: field_label_length bytes (string max 256 chars)
 *         - dtype: 3 chars (string)
 *         - is_array: (number) 0: not array, 1:fixed_length_array, 2:variable_length_array
 *         - array_length: (number max 2^32) (used if is_array == 1)
 *
 * in Binary format:
 *
 * data:
 *     - header fields
 *     - data fields
 *     - header fields
 *     - data fields
 *     ...
 *
 */
struct ClusterFileV3 {
    static constexpr std::string_view MAGIC_STRING = "CLST";
    struct Field {
        std::string label{};
        Dtype dtype{Dtype(Dtype::ERROR)};
        uint8_t is_array{};
        uint32_t array_size{};
    };
    struct Header {
        std::array<char, 3> version;
        uint64_t n_records;
        std::map<std::string, std::string> metadata;
        std::vector<Field> header_fields;
        std::vector<Field> data_fields;

        static Header from_json(std::string const &json) {
            Header header;
            simdjson::padded_string const ps(json.c_str(), json.size());
            simdjson::ondemand::parser parser;
            simdjson::ondemand::document doc = parser.iterate(ps);
            simdjson::ondemand::object object = doc.get_object();
            for (auto field : object) {
                std::string_view const key = field.unescaped_key();
                if (key == "version") {
                    std::string_view version = field.value().get_string();
                    if (version.size() != 3) {
                        throw std::runtime_error("Invalid version string");
                    }
                    std::copy(version.begin(), version.end(), header.version.begin());
                } else if (key == "n_records") {
                    header.n_records = field.value().get_uint64();
                } else if (key == "metadata") {
                    simdjson::ondemand::object metadata = field.value().get_object();
                    for (auto meta : metadata) {
                        std::string_view key_view(meta.unescaped_key());
                        std::string const key_str(key_view.data(), key_view.size());
                        std::string_view value_view(meta.value().get_string());
                        std::string const value_str(value_view.data(), value_view.size());
                        header.metadata[key_str] = value_str;
                    }
                } else if (key == "header_fields") {
                    simdjson::ondemand::array header_fields = field.value().get_array();
                    for (auto hf : header_fields) {
                        Field f;
                        simdjson::ondemand::object field = hf.get_object();
                        for (auto field : field) {
                            std::string_view const key = field.unescaped_key();
                            if (key == "label") {
                                f.label = field.value().get_string().value();
                            } else if (key == "dtype") {
                                f.dtype = Dtype(field.value().get_string().value());
                            } else if (key == "is_array") {
                                f.is_array = field.value().get_uint64();
                            } else if (key == "array_size") {
                                f.array_size = field.value().get_uint64();
                            }
                        }
                    }
                } else if (key == "data_fields") {
                    simdjson::ondemand::array data_fields = field.value().get_array();
                    for (auto df : data_fields) {
                        Field f;
                        simdjson::ondemand::object field = df.get_object();
                        for (auto field : field) {
                            std::string_view const key = field.unescaped_key();
                            if (key == "label") {
                                f.label = field.value().get_string().value();
                            } else if (key == "dtype") {
                                f.dtype = Dtype(field.value().get_string().value());
                            } else if (key == "is_array") {
                                f.is_array = field.value().get_uint64();
                            } else if (key == "array_size") {
                                f.array_size = field.value().get_uint64();
                            }
                        }
                    }
                }
            }
        }
    };

    ClusterFileV3(std::filesystem::path const &fpath, std::string const &mode, Header const &header = Header())
        : m_closed(true), m_fpath(fpath), m_mode(mode), m_header(header) {
        if (mode != "r" && mode != "w")
            throw std::invalid_argument("mode must be 'r' or 'w'");
        if (mode == "r") {
            if (!std::filesystem::exists(fpath)) {
                throw std::invalid_argument("File does not exist");
            }
            m_fp = fopen(fpath.string().c_str(), "rb");
            if (m_fp == nullptr) {
                throw std::runtime_error("Failed to open file");
            }
            std::array<char, MAGIC_STRING.size() /* 4 CLST */> magic_string;
            fread(magic_string.data(), 1, MAGIC_STRING.size(), m_fp);
            if (std::string_view(magic_string.data(), MAGIC_STRING.size()) != MAGIC_STRING) {
                throw std::runtime_error("Invalid file format: magic string mismatch");
            }
            uint32_t header_length;
            fread(&header_length, sizeof(uint32_t), 1, m_fp);
            std::string header_json(header_length, '\0');
            fread(header_json.data(), 1, header_length, m_fp);
            m_header = Header::from_json(header_json);
        }
    }

  private:
    bool m_closed;
    std::filesystem::path m_fpath;
    std::string m_mode;
    FILE *m_fp;
    Header m_header;
};
} // namespace aare