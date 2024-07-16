#pragma once

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
namespace v3 {

struct Field {
    const static int VLEN_ARRAY_SIZE_BYTES = 4;
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
    bool operator==(Field const &other) const {
        return label == other.label && dtype == other.dtype && is_array == other.is_array &&
               array_size == other.array_size;
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

    bool operator==(Header const &other) const {
        return version == other.version && n_records == other.n_records &&
               metadata == other.metadata && header_fields == other.header_fields &&
               data_fields == other.data_fields;
    }
};

template <typename ClusterHeaderType, typename ClusterDataType> struct ClusterFile {
    static constexpr std::string_view MAGIC_STRING = "CLST";
    static constexpr int HEADER_LEN_CHARS = 5;

    struct Result {
        ClusterHeaderType header;
        std::vector<ClusterDataType> data;
    };

    ClusterFile(std::filesystem::path const &fpath, std::string const &mode,
                Header const header = Header{})
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
                header_length =
                    std::stoi(std::string(header_length_arr.data(), header_length_arr.size()));
            } catch (std::invalid_argument &e) {
                throw std::runtime_error("Invalid file format: header length is not a number");
            }

            std::string header_json(header_length, '\0');
            fread(header_json.data(), 1, header_length, m_fp);
            m_header.from_json(header_json);
            if (m_header.header_fields.size() == 0 || m_header.data_fields.size() == 0) {
                // TODO: verify if this condition is needed after finishing the implementation
                throw std::runtime_error(
                    "Invalid file format: header fields or data fields are empty");
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
        } else if (mode == "w") {
            if (m_header == Header{}) {
                throw std::invalid_argument(
                    "Header must be provided when opening file in write mode");
            }
            m_fp = fopen(fpath.string().c_str(), "wb");
            if (m_fp == nullptr) {
                throw std::runtime_error("Failed to open file");
            }
            fwrite(MAGIC_STRING.data(), 1, MAGIC_STRING.size(), m_fp);
            std::string header_json = header.to_json();
            std::string header_len_str = std::to_string(header_json.size());
            header_len_str.insert(0, HEADER_LEN_CHARS - header_len_str.size(), '0');
            fwrite(header_len_str.data(), 1, HEADER_LEN_CHARS, m_fp);
            fwrite(header_json.data(), 1, header_json.size(), m_fp);
        }
        m_cluster_header_size = calculate_fixed_cluster_size(m_header.header_fields);
        m_cluster_data_size = calculate_fixed_cluster_size(m_header.data_fields);
        m_closed = false;
    }

    Header header() const { return m_header; }
    Result read() {

        if (m_mode != "r") {
            throw std::invalid_argument("File not opened in read mode");
        }
        if (m_closed) {
            throw std::invalid_argument("File is closed");
        }

        // read header
        ClusterHeaderType cluster_header;
        // check if structure has vlen array
        if (!m_header_has_vlen_array) {
            // read fixed size data
            // check if we can read directly into the struct
            if constexpr (ClusterHeaderType::has_data()) {
                fread(cluster_header.data(), 1, m_cluster_header_size, m_fp);
            } else {
                // read into a temporary buffer and then call set() from the struct
                std::array<std::byte, sizeof(ClusterHeaderType)> tmp;
                fread(tmp.data(), 1, m_cluster_header_size, m_fp);
                cluster_header.set(tmp.data());
            }

        } else {
            std::vector<std::byte> tmp = read_vlen_array();
            cluster_header.set(tmp.data());
        }

        // read data
        std::vector<ClusterDataType> cluster_data(cluster_header.data_count());
        if (!m_data_has_vlen_array) {
            // read fixed size data
            if constexpr (ClusterDataType::has_data()) {
                // read n_clusters directly into the vector
                fread(cluster_data.data(), cluster_header.data_count(), m_cluster_data_size, m_fp);
            } else {
                std::array<std::byte, sizeof(ClusterDataType)> tmp;
                for (int i = 0; i < cluster_header.data_count(); i++) {
                    fread(tmp.data(), 1, m_cluster_data_size, m_fp);
                    cluster_data[i].set(tmp.data());
                }
            }
        } else {
            for (int i = 0; i < cluster_header.data_count(); i++) {
                std::vector<std::byte> tmp = read_vlen_array();
                cluster_data[i].set(tmp.data());
            }
        }
        return {cluster_header, cluster_data};
    }

    void write(ClusterHeaderType cluster_header, std::vector<ClusterDataType> cluster_data) {
        if (m_mode != "w") {
            throw std::invalid_argument("File not opened in write mode");
        }
        if (m_closed) {
            throw std::invalid_argument("File is closed");
        }
        if (cluster_header.data_count() != cluster_data.size()) {
            throw std::invalid_argument("Number of data records does not match the header");
        }
        // write header
        if (!m_header_has_vlen_array) {
            if constexpr (ClusterHeaderType::has_data()) {
                fwrite(cluster_header.data(), 1, m_cluster_header_size, m_fp);
            } else {
                std::array<std::byte, cluster_header.size()> tmp;
                cluster_header.get(tmp.data());
                fwrite(tmp.data(), 1, cluster_header.size(), m_fp);
            }
        } else {
            std::array<std::byte, cluster_header.size()> tmp;
            cluster_header.get(tmp.data());
            fwrite(tmp.data(), 1, cluster_header.size(), m_fp);
        }
        // write data
        if (!m_data_has_vlen_array) {
            if constexpr (ClusterDataType::has_data()) {
                fwrite(cluster_data.data(), cluster_header.data_count(), m_cluster_data_size, m_fp);
            } else {
                std::array<std::byte, m_cluster_data_size> tmp;
                for (int i = 0; i < cluster_header.data_count(); i++) {
                    cluster_data[i].get(tmp.data());
                    fwrite(tmp.data(), 1, m_cluster_data_size, m_fp);
                }
            }
        } else {
            for(int i=0;i<cluster_header.data_count();i++){
                std::array<std::byte, cluster_data[i].size()> tmp;
                cluster_data[i].get(tmp.data());
                fwrite(tmp.data(), 1, m_cluster_data_size, m_fp);

            }
        }
    }

    int flush() { return fflush(m_fp); }
    int close() {
        int ret{};
        if (!m_closed) {
            ret = fclose(m_fp);
            if (ret == 0) {
                m_closed = true;
            } else {
                throw std::runtime_error("Failed to close file");
            }
        }
        return ret;
    }
    ~ClusterFile() noexcept {
        if (!m_closed) {
            fclose(m_fp);
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
    std::vector<std::byte> read_vlen_array(std::vector<Field> const &fields) {
        std::vector<std::byte> tmp(10000);
        uint32_t cur_byte = 0;
        for (Field &field : fields) {
            if (field.is_array == Field::NOT_ARRAY) {
                if (cur_byte + field.dtype.bytes() > tmp.size()) {
                    tmp.resize(tmp.size() * 2 + field.dtype.bytes());
                }
                fread(tmp.data() + cur_byte, 1, field.dtype.bytes(), m_fp);
                cur_byte += field.dtype.bytes();

            } else if (field.is_array == Field::FIXED_LENGTH_ARRAY) {
                if (cur_byte + field.array_size * field.dtype.bytes() > tmp.size()) {
                    tmp.resize(tmp.size() * 2 + field.array_size * field.dtype.bytes());
                }
                fread(tmp.data() + cur_byte, 1, field.array_size * field.dtype.bytes(), m_fp);
                cur_byte += field.array_size * field.dtype.bytes();

            } else {
                if (cur_byte + Field::VLEN_ARRAY_SIZE_BYTES > tmp.size()) {
                    tmp.resize(tmp.size() * 2 + Field::VLEN_ARRAY_SIZE_BYTES);
                }
                fread(tmp.data() + cur_byte, 1, Field::VLEN_ARRAY_SIZE_BYTES, m_fp);
                uint32_t n_elements = *reinterpret_cast<uint32_t *>(tmp.data() + cur_byte);
                cur_byte += Field::VLEN_ARRAY_SIZE_BYTES;
                if (cur_byte + n_elements * field.dtype.bytes() > tmp.size()) {
                    tmp.resize(tmp.size() * 2 + n_elements * field.dtype.bytes());
                }
                fread(tmp.data() + cur_byte, 1, n_elements * field.dtype.bytes(), m_fp);
                cur_byte += n_elements * field.dtype.bytes();
            }
        }
        return tmp;
    }
};

} // namespace v3
} // namespace aare