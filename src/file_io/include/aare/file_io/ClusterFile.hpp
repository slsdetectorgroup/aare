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
 *     - n_records (9 character string doesn't exceed 4*10^9 unsigned should be padded with zeroes
 *       if less than 10 digits)
 *     - metadata: metadata (json string max 2^16 chars. no nested json objects, only key-value
 *       pairs of strings)
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

struct ClusterFileHeader {
    static constexpr std::string_view CURRENT_VERSION = "0.1";
    static constexpr uint8_t N_RECORDS_CHARS = 10;
    ClusterFileHeader() : version{CURRENT_VERSION}, n_records{} {};
    std::string version;
    uint32_t n_records;
    std::map<std::string, std::string> metadata;
    std::vector<Field> header_fields;
    std::vector<Field> data_fields;

    std::string to_json() const;
    void from_json(std::string const &json);

    bool operator==(ClusterFileHeader const &other) const;
    bool operator!=(ClusterFileHeader const &other) const { return !(*this == other); }
};

template <typename ClusterHeaderType, typename ClusterDataType> struct ClusterFile {
    static constexpr std::string_view MAGIC_STRING = "CLST";
    static constexpr int HEADER_LEN_CHARS = 5;

    struct Result {
        ClusterHeaderType header;
        std::vector<ClusterDataType> data;
    };

    ClusterFile(std::filesystem::path const &fpath, std::string const &mode,
                ClusterFileHeader header = ClusterFileHeader{}, bool old_format = false);

    ClusterFileHeader header() const { return m_header; }
    Result read();
    void write(ClusterHeaderType cluster_header, std::vector<ClusterDataType> cluster_data);
    void set_n_records(uint32_t n_records) { m_n_records = n_records; }
    uint32_t tell() { return m_cur_index; }
    int flush() { return fflush(m_fp); }
    void update_header();
    void reset() {
        m_cur_index = 0;
        fseek(m_fp, M_DATA_POSITION, SEEK_SET);
    }
    int close(bool throws = true);
    ~ClusterFile() noexcept { close(false); }

  private:
    bool m_closed;
    std::filesystem::path m_fpath;
    std::string m_mode;
    FILE *m_fp;
    uint32_t m_n_records;
    uint32_t m_cur_index;
    ClusterFileHeader m_header;
    bool m_old_format;
    bool m_header_has_vlen_array{false};
    bool m_data_has_vlen_array{false};
    uint32_t m_cluster_header_size{0};
    uint32_t m_cluster_data_size{0};
    uint64_t M_DATA_POSITION;

    uint32_t calculate_fixed_cluster_size(std::vector<Field> const &fields);
    std::vector<std::byte> *read_vlen_array(std::vector<Field> const &fields);
};

} // namespace aare