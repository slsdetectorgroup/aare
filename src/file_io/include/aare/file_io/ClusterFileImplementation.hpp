#pragma once
#include "aare/file_io/ClusterFile.hpp"
namespace aare {

// ClusterFile public functions

template <typename ClusterHeaderType, typename ClusterDataType>
ClusterFile<ClusterHeaderType, ClusterDataType>::ClusterFile(std::filesystem::path const &fpath,
                                                             std::string const &mode,
                                                             ClusterFileHeader header,
                                                             bool old_format)
    : m_closed(true), m_fpath(fpath), m_mode(mode), m_n_records{}, m_cur_index{}, m_header(header),
      m_old_format(old_format) {
    if (mode != "r" && mode != "w")
        throw std::invalid_argument("mode must be 'r' or 'w'");
    // open file in old format
    // if old_format is true, the file is opened only in read mode
    // old_format files have no file_header in the file
    if (m_old_format == true) {
        if (mode == "r") {
            m_fp = fopen(fpath.string().c_str(), "rb");
            if (m_fp == nullptr) {
                throw std::runtime_error("Failed to open file");
                if (header == ClusterFileHeader{}) {
                    throw std::invalid_argument("ClusterFileHeader must be provided when opening "
                                                "old format file in read mode");
                }
            }
            m_header = header;
            m_header.n_records = std::numeric_limits<uint32_t>::max();
            m_cur_index = 0;

        } else if (mode == "w") {
            m_fp = fopen(fpath.string().c_str(), "wb");
            if (m_fp == nullptr) {
                throw std::runtime_error("Failed to open file");
            }
            m_header = header;
            m_header.n_records = 0;
            m_cur_index = 0;
        }
    } else if (mode == "r") {
        if (header != ClusterFileHeader{}) {
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
            throw std::runtime_error("Invalid file format: header fields or data fields are empty");
        }
    } else if (mode == "w") {
        if (m_header == ClusterFileHeader{}) {
            throw std::invalid_argument(
                "ClusterFileHeader must be provided when opening file in write mode");
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
    } else {
        throw std::runtime_error("Internal error: Unreachable code");
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
    m_cluster_header_size = calculate_fixed_cluster_size(m_header.header_fields);
    m_cluster_data_size = calculate_fixed_cluster_size(m_header.data_fields);
    m_closed = false;
    M_DATA_POSITION = ftell(m_fp);
}

template <typename ClusterHeaderType, typename ClusterDataType>
typename ClusterFile<ClusterHeaderType, ClusterDataType>::Result
ClusterFile<ClusterHeaderType, ClusterDataType>::read() {

    if (m_mode != "r") {
        throw std::invalid_argument("File not opened in read mode");
    }
    if (m_closed) {
        throw std::invalid_argument("File is closed");
    }
    if (m_cur_index >= m_n_records) {
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
            cluster_header.set_fields(m_header.header_fields);
            std::array<std::byte, sizeof(ClusterHeaderType)> tmp;
            fread(tmp.data(), 1, m_cluster_header_size, m_fp);
            cluster_header.set(tmp.data());
        }

    } else {
        cluster_header.set_fields(m_header.header_fields);
        std::vector<std::byte> *tmp = read_vlen_array(m_header.header_fields);
        cluster_header.set(tmp->data());
        delete tmp;
    }
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
            std::vector<std::byte> tmp(m_cluster_data_size);
            for (int i = 0; i < cluster_header.data_count(); i++) {
                cluster_data[i].set_fields(m_header.data_fields);
                fread(tmp.data(), 1, cluster_data[i].size(), m_fp);
                cluster_data[i].set(tmp.data());
            }
        }
    } else {
        for (int i = 0; i < cluster_header.data_count(); i++) {
            std::vector<std::byte> *tmp = read_vlen_array(m_header.data_fields);
            cluster_data[i].set_fields(m_header.data_fields);
            cluster_data[i].set(tmp->data());
            delete tmp;
        }
    }
    m_cur_index++;
    return {cluster_header, cluster_data};
}

template <typename ClusterHeaderType, typename ClusterDataType>
void ClusterFile<ClusterHeaderType, ClusterDataType>::write(
    ClusterHeaderType cluster_header, std::vector<ClusterDataType> cluster_data) {
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
    m_cur_index++;
}
template <typename ClusterHeaderType, typename ClusterDataType>
void ClusterFile<ClusterHeaderType, ClusterDataType>::update_header() {
    fseek(m_fp, MAGIC_STRING.size(), SEEK_SET);
    m_header.n_records = m_n_records;
    std::string header_json = m_header.to_json();
    std::string header_len_str = std::to_string(header_json.size());
    header_len_str.insert(0, HEADER_LEN_CHARS - header_len_str.size(), '0');
    fwrite(header_len_str.data(), 1, HEADER_LEN_CHARS, m_fp);
    fwrite(header_json.data(), 1, header_json.size(), m_fp);
}
template <typename ClusterHeaderType, typename ClusterDataType>
int ClusterFile<ClusterHeaderType, ClusterDataType>::close(bool throws) {
    int ret{};
    if (!m_closed) {
        if (m_mode == "w" && !m_old_format) {
            update_header();
        }
        ret = fclose(m_fp);
        if (ret == 0) {
            m_closed = true;
        } else if (throws) {
            throw std::runtime_error("Failed to close file");
        }
    }
    return ret;
}

// ClusterFile private functions
template <typename ClusterHeaderType, typename ClusterDataType>
uint32_t ClusterFile<ClusterHeaderType, ClusterDataType>::calculate_fixed_cluster_size(
    std::vector<Field> const &fields) {
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
template <typename ClusterHeaderType, typename ClusterDataType>
std::vector<std::byte> *
ClusterFile<ClusterHeaderType, ClusterDataType>::read_vlen_array(std::vector<Field> const &fields) {
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

} // namespace aare