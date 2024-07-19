#pragma once
#include "aare/core/Dtype.hpp"
#include "aare/utils/json.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <simdjson.h>
#include <stdexcept>
#include <vector>

namespace aare {

/*
 * TODO: Should be removed
 */


/*
 * new Cluster classes
 *
 */

// class to hold the header of the old cluster format
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

/*
Header Interface{
    // mandatory functions
    void set(std::byte *data);
    void get(std::byte *data);
    int data_count() const;
    constexpr size_t size();
    constexpr static bool has_data();

    // optional
    std::byte *data();
    std::string to_string() const;
}

*/
struct ClusterHeader {
    int32_t frame_number;
    int32_t n_clusters;
    ClusterHeader() : frame_number(0), n_clusters(0) {}
    ClusterHeader(int32_t frame_number_, int32_t n_clusters_)
        : frame_number(frame_number_), n_clusters(n_clusters_) {}

    // interface functions (mandatory)
    void set(std::byte *data_) {
        // std::copy(data, data + sizeof(frame_number), &frame_number);
        // std::copy(data + sizeof(frame_number), data + 2 * sizeof(frame_number), &n_clusters);
        std::memcpy(&frame_number, data_, sizeof(frame_number));
        std::memcpy(&n_clusters, data_ + sizeof(frame_number), sizeof(n_clusters));
    }
    void get(std::byte *data_) {
        // std::copy(&frame_number, &frame_number + sizeof(frame_number), data);
        // std::copy(&n_clusters, &n_clusters + sizeof(n_clusters), data + sizeof(frame_number));
        std::memcpy(data_, &frame_number, sizeof(frame_number));
        std::memcpy(data_ + sizeof(frame_number), &n_clusters, sizeof(n_clusters));
    }
    int32_t data_count() const { return n_clusters; }
    // used to indicate that data can be read directly into the struct using data()
    // e.g. fread(header.data(), sizeof(header), 1, file);
    constexpr static bool has_data() { return true; }
    std::byte *data() { return reinterpret_cast<std::byte *>(this); }
    constexpr size_t size() { return sizeof(frame_number) + sizeof(n_clusters); }

    // interface functions (optional)
    std::string to_string() const {
        return "frame_number: " + std::to_string(frame_number) +
               " n_clusters: " + std::to_string(n_clusters);
    }
    static std::vector<Field> get_fields() {
        return {Field{"frame_number", Dtype(Dtype::INT32), Field::NOT_ARRAY, 0},
                Field{"n_clusters", Dtype(Dtype::INT32), Field::NOT_ARRAY, 0}};
    }
};
// class to hold the data of the old cluster format
template <typename DataType = int32_t, int CLUSTER_SIZE = 9> struct ClusterData {
    int16_t x;
    int16_t y;
    std::array<DataType, CLUSTER_SIZE> array;

    ClusterData() : x(0), y(0), array({}) {}
    ClusterData(int16_t x_, int16_t y_, std::array<int32_t, CLUSTER_SIZE> array_)
        : x(x_), y(y_), array(array_) {}
    void set(std::byte *data_) {
        std::memcpy(&x, data_, sizeof(x));
        std::memcpy(&y, data_ + sizeof(x), sizeof(y));
        std::memcpy(array.data(), data_ + 2 * sizeof(x), CLUSTER_SIZE * sizeof(DataType));
    }
    void get(std::byte *data_) {
        std::memcpy(data_, &x, sizeof(x));
        std::memcpy(data_ + sizeof(x), &y, sizeof(y));
        std::memcpy(data_ + 2 * sizeof(x), array.data(), CLUSTER_SIZE * sizeof(DataType));
    }
    static constexpr bool has_data() { return true; }
    std::byte *data() { return reinterpret_cast<std::byte *>(this); }
    static constexpr size_t size() {
        return sizeof(x) + sizeof(x) + CLUSTER_SIZE * sizeof(DataType);
    }

    std::string to_string() const {
        std::string s = "x: " + std::to_string(x) + " y: " + std::to_string(y) + "\ndata: [";
        for (auto &d : array) {
            s += std::to_string(d) + " ";
        }
        s += "]";
        return s;
    }
    static std::vector<Field> get_fields() {
        return {Field{"x", Dtype(Dtype::INT16), Field::NOT_ARRAY, 0},
                Field{"y", Dtype(Dtype::INT16), Field::NOT_ARRAY, 0},
                Field{"data", Dtype(typeid(DataType)), Field::FIXED_LENGTH_ARRAY, CLUSTER_SIZE}};
    }
};

// vlen cluster data
struct ClusterDataVlen {
    std::vector<int16_t> x;
    std::vector<int16_t> y;
    std::vector<int32_t> energy;

    ClusterDataVlen() : x({}), y({}), energy({}) {}
    ClusterDataVlen(std::vector<int16_t> x_, std::vector<int16_t> y_, std::vector<int32_t> energy_)
        : x(x_), y(y_), energy(energy_) {}
    void set(std::byte *data_) {
        uint32_t n = 0;
        size_t offset = 0;
        // read m_x array size
        std::memcpy(&n, data_, sizeof(n));
        x.resize(n);
        // read m_x array
        std::memcpy(x.data(), data_ + sizeof(n), n * sizeof(int16_t));
        offset = sizeof(n) + n * sizeof(int16_t);
        // read m_y array size
        std::memcpy(&n, data_ + offset, sizeof(n));
        y.resize(n);
        // read m_y array
        std::memcpy(y.data(), data_ + offset + sizeof(n), n * sizeof(int16_t));
        offset += sizeof(n) + n * sizeof(int16_t);
        // read m_energy array size
        std::memcpy(&n, data_ + offset, sizeof(n));
        energy.resize(n);
        // read m_energy array
        std::memcpy(energy.data(), data_ + offset + sizeof(n), n * sizeof(int32_t));
    }
    void get(std::byte *data_) {
        uint32_t n = x.size();
        // write m_x array size
        std::memcpy(data_, &n, sizeof(n));
        // write m_x array
        std::memcpy(data_ + sizeof(n), x.data(), n * sizeof(int16_t));
        size_t offset = sizeof(n) + n * sizeof(int16_t);
        n = y.size();
        // write m_y array size
        std::memcpy(data_ + offset, &n, sizeof(n));
        // write m_y array
        std::memcpy(data_ + offset + sizeof(n), y.data(), n * sizeof(int16_t));
        offset += sizeof(n) + n * sizeof(int16_t);
        n = energy.size();
        // write m_energy array size
        std::memcpy(data_ + offset, &n, sizeof(n));
        // write m_energy array
        std::memcpy(data_ + offset + sizeof(n), energy.data(), n * sizeof(int32_t));
    }
    size_t size() const {
        return sizeof(uint32_t) + x.size() * sizeof(int16_t) + sizeof(uint32_t) +
               y.size() * sizeof(int16_t) + sizeof(uint32_t) + energy.size() * sizeof(int32_t);
    }
    constexpr static bool has_data() { return false; }

    static std::vector<Field> get_fields() {
        return {Field{"x", Dtype(Dtype::INT16), Field::VARIABLE_LENGTH_ARRAY, 0},
                Field{"y", Dtype(Dtype::INT16), Field::VARIABLE_LENGTH_ARRAY, 0},
                Field{"data", Dtype(Dtype::INT32), Field::VARIABLE_LENGTH_ARRAY, 0}};
    }
    std::string to_string() const {
        std::string s = "x: [";
        for (auto &d : x) {
            s += std::to_string(d) + ", ";
        }
        s += "]\ny: [";
        for (auto &d : y) {
            s += std::to_string(d) + ", ";
        }
        s += "]\nenergy: [";
        for (auto &d : energy) {
            s += std::to_string(d) + ", ";
        }
        s += "]";
        return s;
    }
};

} // namespace aare