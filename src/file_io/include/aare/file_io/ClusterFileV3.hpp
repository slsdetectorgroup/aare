#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "simdjson.h"

#include "aare/core/Cluster.hpp"
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
 *     - n_records (string doesn't exceed 4*10^9 unsigned should be padded with zeroes if less than
 * 9 digits)
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

struct Header {
    static constexpr std::string_view CURRENT_VERSION = "0.1";
    Header() : version{CURRENT_VERSION}, n_records{} {};
    std::string version;
    uint32_t n_records;
    std::map<std::string, std::string> metadata;
    std::vector<Field> header_fields;
    std::vector<Field> data_fields;

    std::string to_json() const {
        std::string json;
        json.reserve(1024);
        json += "{";
        write_str(json, "version", version);
        std::string n_records_str = std::to_string(n_records);
        n_records_str.insert(0, 9 - n_records_str.size(), '0');
        write_str(json, "n_records", n_records_str);

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
                std::string_view tmp = field.value().get_string();
                try {

                    this->n_records = std::stoi(std::string(tmp.data(), tmp.size()));
                } catch (std::invalid_argument &e) {
                    throw std::runtime_error("Invalid n_records value");
                }

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
    bool operator!=(Header const &other) const { return !(*this == other); }
};

template <typename ClusterHeaderType, typename ClusterDataType> struct ClusterFile {
    static constexpr std::string_view MAGIC_STRING = "CLST";
    static constexpr int HEADER_LEN_CHARS = 5;

    struct Result {
        ClusterHeaderType header;
        std::vector<ClusterDataType> data;
    };

    ClusterFile(std::filesystem::path const &fpath, std::string const &mode,
                Header header = Header{})
        : m_closed(true), m_fpath(fpath), m_mode(mode), m_n_records{},m_cur_index{}, m_header(header) {
        if (mode != "r" && mode != "w")
            throw std::invalid_argument("mode must be 'r' or 'w'");
        if (mode == "r") {
            if (header != Header{}) {
                header = {};
                logger::warn("Ignoring provided header when opening file in read mode");
            }
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
            m_n_records = m_header.n_records;
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
        if( m_cur_index >= m_n_records){
            throw std::invalid_argument("End of file reached");
        }

        /***************************
         *** read cluster header ***
         ***************************
         */
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
            std::vector<std::byte> *tmp = read_vlen_array(m_header.header_fields);
            cluster_header.set(tmp->data());
            delete tmp;
        }
        cluster_header.data_count();
        /*************************
         *** read cluster data ***
         *************************
         */
        std::vector<ClusterDataType> cluster_data(cluster_header.data_count());
        if (!m_data_has_vlen_array) {
            // read fixed size data
            if constexpr (ClusterDataType::has_data()) {
                // read n_clusters directly into the vector
                fread(cluster_data.data(), cluster_header.data_count(), m_cluster_data_size, m_fp);
            } else {
                std::array<std::byte, sizeof(ClusterDataType)> tmp;
                for (int i = 0; i < cluster_header.data_count(); i++) {
                    fread(tmp.data(), 1, cluster_data[i].size(), m_fp);
                    cluster_data[i].set(tmp.data());
                }
            }
        } else {
            for (int i = 0; i < cluster_header.data_count(); i++) {
                std::vector<std::byte> *tmp = read_vlen_array(m_header.data_fields);
                cluster_data[i].set(tmp->data());
                delete tmp;
            }
        }
        m_cur_index++;
        return {cluster_header, cluster_data};
    }

    void write(ClusterHeaderType cluster_header, std::vector<ClusterDataType> cluster_data) {
        if (m_mode != "w") {
            throw std::invalid_argument("File not opened in write mode");
        }
        if (m_closed) {
            throw std::invalid_argument("File is closed");
        }
        if (static_cast<uint64_t>(cluster_header.data_count()) != cluster_data.size()) {
            throw std::invalid_argument("Number of data records does not match the header");
        }

        /****************************
         *** write cluster header ***
         ****************************
         */
        if (ClusterHeaderType::has_data() && !m_header_has_vlen_array) {
            fwrite(cluster_header.data(), 1, cluster_header.size(), m_fp);
        } else {
            // either the header has vlen array or we can't read into the struct directly
            auto tmp = new std::byte[cluster_header.size()];
            cluster_header.get(tmp);
            fwrite(tmp, 1, cluster_header.size(), m_fp);
            delete[] tmp;
        }

        /**************************
         *** write cluster data ***
         **************************
         */
        if (ClusterDataType::has_data() && !m_data_has_vlen_array) {
            fwrite(cluster_data.data(), cluster_header.data_count(), m_cluster_data_size, m_fp);
        } else {
            for (int i = 0; i < cluster_header.data_count(); i++) {
                auto tmp = new std::byte[cluster_data[i].size()];
                cluster_data[i].get(tmp);
                fwrite(tmp, 1, cluster_data[i].size(), m_fp);
                delete[] tmp;
            }
        }
        m_n_records++;
    }
    void set_n_records(uint32_t n_records) { m_n_records = n_records; }

    int flush() { return fflush(m_fp); }
    void update_header() {
        fseek(m_fp, MAGIC_STRING.size(), SEEK_SET);
        m_header.n_records = m_n_records;
        std::string header_json = m_header.to_json();
        std::string header_len_str = std::to_string(header_json.size());
        header_len_str.insert(0, HEADER_LEN_CHARS - header_len_str.size(), '0');
        fwrite(header_len_str.data(), 1, HEADER_LEN_CHARS, m_fp);
        fwrite(header_json.data(), 1, header_json.size(), m_fp);
    }
    int close(bool throws = true) {
        int ret{};
        if (!m_closed) {
            update_header();
            ret = fclose(m_fp);
            if (ret == 0) {
                m_closed = true;
            } else if (throws) {
                throw std::runtime_error("Failed to close file") ;
            }
        }
        return ret;
    }
    ~ClusterFile() noexcept { close(false); }

  private:
    bool m_closed;
    std::filesystem::path m_fpath;
    std::string m_mode;
    FILE *m_fp;
    uint32_t m_n_records;
    u_int32_t m_cur_index;
    Header m_header;
    bool m_header_has_vlen_array{false};
    bool m_data_has_vlen_array{false};
    uint32_t m_cluster_header_size{0};
    uint32_t m_cluster_data_size{0};

    uint32_t calculate_fixed_cluster_size(std::vector<Field> const &fields) {
        uint32_t size = 0;
        for (auto &f : fields) {
            if (f.is_array == Field::NOT_ARRAY) {
                size += f.dtype.bytes();
            } else if (f.is_array == Field::FIXED_LENGTH_ARRAY) {
                size += f.array_size * f.dtype.bytes();
            } else {
                return 0;
            }
        }
        return size;
    }
    std::vector<std::byte> *read_vlen_array(std::vector<Field> const &fields) {
        std::vector<std::byte> *tmp = new std::vector<std::byte>(1000);
        uint32_t cur_byte = 0;
        for (const Field &field : fields) {
            if (field.is_array == Field::NOT_ARRAY) {
                if (cur_byte + field.dtype.bytes() > tmp->size()) {
                    tmp->resize(tmp->size() * 2 + field.dtype.bytes());
                }
                fread(tmp->data() + cur_byte, 1, field.dtype.bytes(), m_fp);
                cur_byte += field.dtype.bytes();

            } else if (field.is_array == Field::FIXED_LENGTH_ARRAY) {
                if (cur_byte + field.array_size * field.dtype.bytes() > tmp->size()) {
                    tmp->resize(tmp->size() * 2 + field.array_size * field.dtype.bytes());
                }
                fread(tmp->data() + cur_byte, 1, field.array_size * field.dtype.bytes(), m_fp);
                cur_byte += field.array_size * field.dtype.bytes();

            } else {
                if (cur_byte + Field::VLEN_ARRAY_SIZE_BYTES > tmp->size()) {
                    tmp->resize(tmp->size() * 2 + Field::VLEN_ARRAY_SIZE_BYTES);
                }
                uint32_t n_elements;
                fread(&n_elements, 1, Field::VLEN_ARRAY_SIZE_BYTES, m_fp);
                memcpy(tmp->data() + cur_byte, &n_elements, Field::VLEN_ARRAY_SIZE_BYTES);
                cur_byte += Field::VLEN_ARRAY_SIZE_BYTES;
                if (cur_byte + n_elements * field.dtype.bytes() > tmp->size()) {
                    tmp->resize(tmp->size() * 2 + n_elements * field.dtype.bytes());
                }
                fread(tmp->data() + cur_byte, 1, n_elements * field.dtype.bytes(), m_fp);
                cur_byte += n_elements * field.dtype.bytes();
            }
        }
        return tmp;
    }
};

} // namespace v3
} // namespace aare