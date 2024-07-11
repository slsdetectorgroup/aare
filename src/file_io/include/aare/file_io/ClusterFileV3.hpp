#include <filesystem>
#include <string>
#include <vector>

#include "simdjson.h"

#include "aare/core/Dtype.hpp"
#include "aare/core/defs.hpp"
#include "aare/utils/json.hpp"
namespace aare {
/**
 * in TEXT format:
 *    - magic_string: 4 characters ("CLST")
 *    - header_length: 5 characters (max 99999, must be prefixed with zeros if less than 5 digits
 * (00123))
 *
 * in JSON format:
 *     - version: "0.1" (string max 3 chars)
 *     - n_records (number doesn't exceed 2^64 unsigned)
 *     - metadata: metadata (json string max 2^16 chars. no nested json objects, only key-value
 * pairs of strings)
 *     - header_fields (array of field objects):
 *         - field_label: field_label_length bytes (string max 256 chars)
 *         - dtype: 3 chars (string)
 *         - is_array: (number) 0: not array, 1:fixed_length_array, 2:variable_length_array
 *         - array_length: (number max 2^32) (used if is_array == 1)
 *
 *         if field is:
 *            - not array: value of the field (dtype.bytes())
 *            - fixed_length_array: array_length * dtype.bytes()
 *            - variable_length_array: 4 bytes (number of elements) + number_of_elements *
 * dtype.bytes()
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
    static constexpr int HEADER_LEN_CHARS = 5;
    struct Field {
        enum ARRAY_TYPE { NOT_ARRAY = 0, FIXED_LENGTH_ARRAY = 1, VARIABLE_LENGTH_ARRAY = 2 };
        static ARRAY_TYPE to_array_type(uint64_t i) {
            if (i == 0)
                return NOT_ARRAY;
            if (i == 1)
                return FIXED_LENGTH_ARRAY;
            if (i == 2)
                return VARIABLE_LENGTH_ARRAY;
            throw std::invalid_argument("Invalid ARRAY_TYPE");
        }
        Field() = default;
        Field(std::string const &label_, Dtype dtype_, ARRAY_TYPE is_array_, uint32_t array_size_)
            : label(label_), dtype(dtype_), is_array(is_array_), array_size(array_size_) {}
        std::string label{};
        Dtype dtype{Dtype(Dtype::ERROR)};
        ARRAY_TYPE is_array{};
        uint32_t array_size{};

        std::string to_json() const {
            std::string json;
            json.reserve(100);
            json += "{";
            write_str(json, "label", label);
            write_str(json, "dtype", dtype.to_string());
            write_digit(json, "is_array", is_array);
            write_digit(json, "array_size", array_size);
            json.pop_back();
            json.pop_back();
            json += "}";
            return json;
        }
        void from_json(std::string hf) {
            simdjson::padded_string const ps(hf.c_str(), hf.size());
            simdjson::ondemand::parser parser;
            simdjson::ondemand::document doc = parser.iterate(ps);
            simdjson::ondemand::object object = doc.get_object();

            for (auto field : object) {
                std::string_view const key = field.unescaped_key();
                if (key == "label") {
                    this->label = field.value().get_string().value();
                } else if (key == "dtype") {
                    this->dtype = Dtype(field.value().get_string().value());
                } else if (key == "is_array") {
                    this->is_array = Field::to_array_type(field.value().get_uint64());
                } else if (key == "array_size") {
                    this->array_size = field.value().get_uint64();
                }
            }
        }
    };
    struct Header {
        static constexpr std::string_view CURRENT_VERSION = "0.1";
        Header() : version{CURRENT_VERSION}, n_records{} {};
        std::string version;
        uint64_t n_records;
        std::map<std::string, std::string> metadata;
        std::vector<Field> header_fields;
        std::vector<Field> data_fields;

        std::string to_json() const {
            std::string json;
            json.reserve(1024);
            json += "{";
            write_str(json, "version", version);
            write_digit(json, "n_records", n_records);
            write_map(json, "metadata", metadata);
            json += "\"header_fields\": [";
            for (auto &f : header_fields) {
                json += f.to_json();
                json += ", ";
            }
            json.pop_back();
            json.pop_back();
            json += "], ";
            json += "\"data_fields\": [";
            for (auto &f : data_fields) {
                json += f.to_json();
                json += ", ";
            }
            json.pop_back();
            json.pop_back();
            json += "]";
            json += "}";
            return json;
        }

        void from_json(std::string const &json) {
            simdjson::padded_string const ps(json.c_str(), json.size());
            simdjson::ondemand::parser parser;
            simdjson::ondemand::document doc = parser.iterate(ps);
            simdjson::ondemand::object object = doc.get_object();
            for (auto field : object) {
                std::string_view const key = field.unescaped_key();
                if (key == "version") {
                    std::string_view version_sv = field.value().get_string();
                    if (version_sv.size() != 3) {
                        throw std::runtime_error("Invalid version string");
                    }
                    this->version = std::string(version_sv.data(), version_sv.size());
                } else if (key == "n_records") {
                    this->n_records = field.value().get_uint64();
                } else if (key == "metadata") {
                    simdjson::ondemand::object metadata_obj = field.value().get_object();
                    for (auto meta : metadata_obj) {
                        std::string_view key_view(meta.unescaped_key());
                        std::string const key_str(key_view.data(), key_view.size());
                        std::string_view value_view(meta.value().get_string());
                        std::string const value_str(value_view.data(), value_view.size());
                        this->metadata[key_str] = value_str;
                    }
                } else if (key == "header_fields") {
                    simdjson::ondemand::array header_fields_arr = field.value().get_array();
                    for (auto hf : header_fields_arr) {
                        Field f;
                        std::string_view const sv = hf.raw_json();
                        f.from_json(std::string(sv.data(), sv.size()));
                        this->header_fields.push_back(f);
                    }
                } else if (key == "data_fields") {
                    simdjson::ondemand::array data_fields_arr = field.value().get_array();
                    for (auto df : data_fields_arr) {
                        Field f;
                        std::string_view const sv = df.raw_json();
                        f.from_json(std::string(sv.data(), sv.size()));
                        this->data_fields.push_back(f);
                    }
                }
            }
        }
    };

    ClusterFileV3(std::filesystem::path const &fpath, std::string const &mode, Header const header = Header{})
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
            std::array<char, HEADER_LEN_CHARS> header_length_arr;
            fread(header_length_arr.data(), header_length_arr.size(), 1, m_fp);
            uint32_t header_length;
            try {
                header_length = std::stoi(std::string(header_length_arr.data(), header_length_arr.size()));
            } catch (std::invalid_argument &e) {
                throw std::runtime_error("Invalid file format: header length is not a number");
            }

            std::string header_json(header_length, '\0');
            fread(header_json.data(), 1, header_length, m_fp);
            m_header.from_json(header_json);
            if (header.header_fields.size() == 0 || header.data_fields.size() == 0) {
                // TODO: verify if this condition is needed after finishing the implementation
                throw std::runtime_error("Invalid file format: header fields or data fields are empty");
            }
            for (auto &f : m_header.header_fields) {
                if (f.is_array == 2) {
                    m_header_has_vlen_array = true;
                    break;
                }
            }
            for (auto &f : m_header.data_fields) {
                if (f.is_array == 2) {
                    m_data_has_vlen_array = true;
                    break;
                }
            }
            m_cluster_header_size = calculate_fixed_cluster_size(m_header.header_fields);
            m_cluster_data_size = calculate_fixed_cluster_size(m_header.data_fields);
        }
    }

  private:
    bool m_closed;
    std::filesystem::path m_fpath;
    std::string m_mode;
    FILE *m_fp;
    Header m_header;
    bool m_header_has_vlen_array{false};
    bool m_data_has_vlen_array{false};
    uint32_t m_cluster_header_size{0};
    uint32_t m_cluster_data_size{0};

    uint32_t calculate_fixed_cluster_size(std::vector<Field> const &fields) {
        uint32_t size = 0;
        for (auto &f : fields) {
            if (f.is_array == 0) {
                size += f.dtype.bytes();
            } else if (f.is_array == 1) {
                size += f.array_size * f.dtype.bytes();
            } else {
                throw std::runtime_error("Calculating Fixed size of variable length array");
            }
        }
        return size;
    }
};
} // namespace aare