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

namespace deprecated {

class Cluster {
  public:
    int cluster_sizeX;
    int cluster_sizeY;
    int16_t x;
    int16_t y;
    Dtype dt;

  private:
    std::byte *m_data;

  public:
    Cluster(int cluster_sizeX_, int cluster_sizeY_, Dtype dt_ = Dtype(typeid(int32_t)))
        : cluster_sizeX(cluster_sizeX_), cluster_sizeY(cluster_sizeY_), dt(dt_) {
        m_data = new std::byte[cluster_sizeX * cluster_sizeY * dt.bytes()]{};
    }
    Cluster() : Cluster(3, 3) {}
    Cluster(const Cluster &other) : Cluster(other.cluster_sizeX, other.cluster_sizeY, other.dt) {
        if (this == &other)
            return;
        x = other.x;
        y = other.y;
        std::memcpy(m_data, other.m_data, other.bytes());
    }
    Cluster &operator=(const Cluster &other) {
        if (this == &other)
            return *this;
        this->~Cluster();
        new (this) Cluster(other);
        return *this;
    }
    Cluster(Cluster &&other) noexcept
        : cluster_sizeX(other.cluster_sizeX), cluster_sizeY(other.cluster_sizeY), x(other.x),
          y(other.y), dt(other.dt), m_data(other.m_data) {
        other.m_data = nullptr;
        other.dt = Dtype(Dtype::TypeIndex::ERROR);
    }
    ~Cluster() { delete[] m_data; }
    template <typename T> T get(int idx) {
        (sizeof(T) == dt.bytes()) ? 0 : throw std::invalid_argument("[ERROR] Type size mismatch");
        return *reinterpret_cast<T *>(m_data + idx * dt.bytes());
    }
    template <typename T> auto set(int idx, T val) {
        (sizeof(T) == dt.bytes()) ? 0 : throw std::invalid_argument("[ERROR] Type size mismatch");
        return memcpy(m_data + idx * dt.bytes(), &val, (size_t)dt.bytes());
    }
    // auto x() const { return x; }
    // auto y() const { return y; }
    // auto x(int16_t x_) { return x = x_; }
    // auto y(int16_t y_) { return y = y_; }

    template <typename T> std::string to_string() const {
        (sizeof(T) == dt.bytes()) ? 0 : throw std::invalid_argument("[ERROR] Type size mismatch");
        std::string s = "x: " + std::to_string(x) + " y: " + std::to_string(y) + "\nm_data: [";
        for (int i = 0; i < cluster_sizeX * cluster_sizeY; i++) {
            s += std::to_string(*reinterpret_cast<T *>(m_data + i * dt.bytes())) + " ";
        }
        s += "]";
        return s;
    }
    /**
     * @brief size of the cluster in bytes when saved to a file
     */
    size_t size() const { return cluster_sizeX * cluster_sizeY; }
    size_t bytes() const { return cluster_sizeX * cluster_sizeY * dt.bytes(); }
    auto begin() const { return m_data; }
    auto end() const { return m_data + cluster_sizeX * cluster_sizeY * dt.bytes(); }
    std::byte *data() { return m_data; }
};
} // namespace deprecated

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
template <typename ArrayType> class Cluster {
    void* m_cluster{nullptr};
    Dtype m_dt;
 
    void f(template <typename T> std::function<void()> function){
        if constexpr (std::is_same<ArrayType, int32_t>::value) {
            function<T>();
        } else if constexpr (std::is_same<ArrayType, int16_t>::value) {
            function<T>();
        } 
        else if(std::is_same<ArrayType, float>::value){
            function<T>();
        }
        else if(std::is_same<ArrayType, double>::value){
            function<T>();
        }
        else if (std::is_same<ArrayType, int8_t>::value) {
            function<T>();
        }else if (std::is_same<ArrayType, int16_t>::value) {
            function<T>();
    }else if(std::is_same<ArrayType, uint32_t>::value){
        function<T>();
    }
      
        
    }
  public:
    Cluster(int cluster_size, Dtype dt) : m_dt(dt){
        f([this, cluster_size](){
            m_cluster = new ClusterData<ArrayType, >();
        });
        
    }
    int16_t x() const { return m_cluster->x; }
    int16_t y() const { return m_cluster->y; }
    void x(int16_t x_) { m_cluster->x = x_; }
    void y(int16_t y_) { m_cluster->y = y_; }
    void set(int idx, ArrayType val) { m_cluster->set(idx, &val); }
    ArrayType get(int idx) {
        return *reinterpret_cast<ArrayType *>(m_cluster->data() + idx * sizeof(ArrayType));
    }
    std::string to_string() const { return m_cluster->to_string(); }
    void set(std::byte *data) { m_cluster->set(data); }
    void get(std::byte *data) { m_cluster->get(data); }
    std::byte *data() { return m_cluster->data(); }
    size_t size() { return m_cluster->size(); }
    ~Cluster() { delete m_cluster; }
};
struct ClusterInterface {
    // mandatory functions
    virtual void set(int idx, void *val) = 0;
    virtual bool has_data() { return false; };
    virtual std::byte *data() { return nullptr; };
    virtual std::string to_string() const { return "Cluster"; };
};
}
// class to hold the data of the old cluster format
template <typename DataType = int32_t, int CLUSTER_SIZE = 9>
struct ClusterData : public aare::ClusterFinderClusterInterface {
    int16_t m_x;
    int16_t m_y;
    std::array<DataType, CLUSTER_SIZE> m_data;

    ClusterData() : m_x(0), m_y(0), m_data({}) {}
    ClusterData(int16_t x_, int16_t y_, std::array<int32_t, CLUSTER_SIZE> data_)
        : m_x(x_), m_y(y_), m_data(data_) {}
    virtual void set(std::byte *data_) override {
        std::memcpy(&m_x, data_, sizeof(m_x));
        std::memcpy(&m_y, data_ + sizeof(m_x), sizeof(m_y));
        std::memcpy(m_data.data(), data_ + 2 * sizeof(m_x), CLUSTER_SIZE * sizeof(DataType));
    }
    virtual void get(std::byte *data_) override {
        std::memcpy(data_, &m_x, sizeof(m_x));
        std::memcpy(data_ + sizeof(m_x), &m_y, sizeof(m_y));
        std::memcpy(data_ + 2 * sizeof(m_x), m_data.data(), CLUSTER_SIZE * sizeof(DataType));
    }
    virtual constexpr bool has_data() override { return true; }
    std::byte *data() { return reinterpret_cast<std::byte *>(this); }
    constexpr size_t size() { return sizeof(m_x) + sizeof(m_x) + CLUSTER_SIZE * sizeof(DataType); }

    std::string to_string() const {
        std::string s = "x: " + std::to_string(m_x) + " y: " + std::to_string(m_y) + "\ndata: [";
        for (auto &d : m_data) {
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