#pragma once

#include "aare/Dtype.hpp"
#include "aare/type_traits.hpp"

#include <array>
#include <stdexcept>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <iostream>
#include <sstream>  
#include <optional>
#include <chrono>

/**
 * @brief LOCATION macro to get the current location in the code
 */
#define LOCATION                                                               \
    std::string(__FILE__) + std::string(":") + std::to_string(__LINE__) +      \
        ":" + std::string(__func__) + ":"



#ifdef AARE_CUSTOM_ASSERT
#define AARE_ASSERT(expr)\
    if (expr)\
        {}\
    else\
        aare::assert_failed(LOCATION + " Assertion failed: " + #expr + "\n");
#else
#define AARE_ASSERT(cond)\
    do { (void)sizeof(cond); } while(0)
#endif


namespace aare {

inline constexpr size_t bits_per_byte = 8;

void assert_failed(const std::string &msg);



class DynamicCluster {
  public:
    int cluster_sizeX;
    int cluster_sizeY;
    int16_t x;
    int16_t y;
    Dtype dt; // 4 bytes

  private:
    std::byte *m_data;

  public:
    DynamicCluster(int cluster_sizeX_, int cluster_sizeY_,
            Dtype dt_ = Dtype(typeid(int32_t)))
        : cluster_sizeX(cluster_sizeX_), cluster_sizeY(cluster_sizeY_),
          dt(dt_) {
        m_data = new std::byte[cluster_sizeX * cluster_sizeY * dt.bytes()]{};
    }
    DynamicCluster() : DynamicCluster(3, 3) {}
    DynamicCluster(const DynamicCluster &other)
        : DynamicCluster(other.cluster_sizeX, other.cluster_sizeY, other.dt) {
        if (this == &other)
            return;
        x = other.x;
        y = other.y;
        memcpy(m_data, other.m_data, other.bytes());
    }
    DynamicCluster &operator=(const DynamicCluster &other) {
        if (this == &other)
            return *this;
        this->~DynamicCluster();
        new (this) DynamicCluster(other);
        return *this;
    }
    DynamicCluster(DynamicCluster &&other) noexcept
        : cluster_sizeX(other.cluster_sizeX),
          cluster_sizeY(other.cluster_sizeY), x(other.x), y(other.y),
          dt(other.dt), m_data(other.m_data) {
        other.m_data = nullptr;
        other.dt = Dtype(Dtype::TypeIndex::ERROR);
    }
    ~DynamicCluster() { delete[] m_data; }
    template <typename T> T get(int idx) {
        (sizeof(T) == dt.bytes())
            ? 0
            : throw std::invalid_argument("[ERROR] Type size mismatch");
        return *reinterpret_cast<T *>(m_data + idx * dt.bytes());
    }
    template <typename T> auto set(int idx, T val) {
        (sizeof(T) == dt.bytes())
            ? 0
            : throw std::invalid_argument("[ERROR] Type size mismatch");
        return memcpy(m_data + idx * dt.bytes(), &val, dt.bytes());
    }

    template <typename T> std::string to_string() const {
        (sizeof(T) == dt.bytes())
            ? 0
            : throw std::invalid_argument("[ERROR] Type size mismatch");
        std::string s = "x: " + std::to_string(x) + " y: " + std::to_string(y) +
                        "\nm_data: [";
        for (int i = 0; i < cluster_sizeX * cluster_sizeY; i++) {
            s += std::to_string(
                     *reinterpret_cast<T *>(m_data + i * dt.bytes())) +
                 " ";
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
    auto end() const {
        return m_data + cluster_sizeX * cluster_sizeY * dt.bytes();
    }
    std::byte *data() { return m_data; }
};

/**
 * @brief header contained in parts of frames
 */
struct DetectorHeader {
    uint64_t frameNumber;
    uint32_t expLength;
    uint32_t packetNumber;
    uint64_t bunchId;
    uint64_t timestamp;
    uint16_t modId;
    uint16_t row;
    uint16_t column;
    uint16_t reserved;
    uint32_t debug;
    uint16_t roundRNumber;
    uint8_t detType;
    uint8_t version;
    std::array<uint8_t, 64> packetMask;
    std::string to_string() {
        std::string packetMaskStr = "[";
        for (auto &i : packetMask) {
            packetMaskStr += std::to_string(i) + ", ";
        }
        packetMaskStr += "]";

        return "frameNumber: " + std::to_string(frameNumber) + "\n" +
               "expLength: " + std::to_string(expLength) + "\n" +
               "packetNumber: " + std::to_string(packetNumber) + "\n" +
               "bunchId: " + std::to_string(bunchId) + "\n" +
               "timestamp: " + std::to_string(timestamp) + "\n" +
               "modId: " + std::to_string(modId) + "\n" +
               "row: " + std::to_string(row) + "\n" +
               "column: " + std::to_string(column) + "\n" +
               "reserved: " + std::to_string(reserved) + "\n" +
               "debug: " + std::to_string(debug) + "\n" +
               "roundRNumber: " + std::to_string(roundRNumber) + "\n" +
               "detType: " + std::to_string(detType) + "\n" +
               "version: " + std::to_string(version) + "\n" +
               "packetMask: " + packetMaskStr + "\n";
    }
};

template <typename T> struct t_xy {
    T row;
    T col;
    bool operator==(const t_xy &other) const {
        return row == other.row && col == other.col;
    }
    bool operator!=(const t_xy &other) const { return !(*this == other); }
    std::string to_string() const {
        return "{ x: " + std::to_string(row) + " y: " + std::to_string(col) +
               " }";
    }
};
using xy = t_xy<uint32_t>;


/**
 * @brief Class to hold the geometry of a module. Where pixel 0 is located and the size of the module
 */
struct ModuleGeometry{
    int origin_x{};
    int origin_y{};
    int height{};
    int width{};
    int row_index{};
    int col_index{}; 
};

/**
 * @brief Class to hold the geometry of a detector. Number of modules, their size and where pixel 0 
 * for each module is located
 */
struct DetectorGeometry{
    int modules_x{};
    int modules_y{};
    int pixels_x{};
    int pixels_y{};
    int module_gap_row{};
    int module_gap_col{};
    std::vector<ModuleGeometry> module_pixel_0;
    
    auto size() const { return module_pixel_0.size(); }
};

struct ROI{
    ssize_t xmin{};
    ssize_t xmax{};
    ssize_t ymin{};
    ssize_t ymax{};
  
    ssize_t height() const { return ymax - ymin; }
    ssize_t width() const { return xmax - xmin; }
    bool contains(ssize_t x, ssize_t y) const {
        return x >= xmin && x < xmax && y >= ymin && y < ymax;
    }
  };


using dynamic_shape = std::vector<ssize_t>;


class ScanParameters {
    bool m_enabled = false;
    std::string m_dac;
    int m_start = 0;
    int m_stop = 0;
    int m_step = 0;
    //TODO! add settleTime, requires string to time conversion

  public:
    // "[enabled\ndac dac 4\nstart 500\nstop 2200\nstep 5\nsettleTime 100us\n]"
    ScanParameters(const std::string &par) {
        std::istringstream iss(par.substr(1, par.size()-2));
        std::string line;
        while(std::getline(iss, line)){
            if(line == "enabled"){
                m_enabled = true;
            }else if(line.find("dac") != std::string::npos){
                m_dac = line.substr(4);
            }else if(line.find("start") != std::string::npos){
                m_start = std::stoi(line.substr(6));
            }else if(line.find("stop") != std::string::npos){
                m_stop = std::stoi(line.substr(5));
            }else if(line.find("step") != std::string::npos){
                m_step = std::stoi(line.substr(5));
            }
        }   
    };
    ScanParameters() = default;
    ScanParameters(const ScanParameters &) = default;
    ScanParameters &operator=(const ScanParameters &) = default;
    ScanParameters(ScanParameters &&) = default;
    int start() const { return m_start; };
    int stop() const { return m_stop; };
    int step() const { return m_step; };
    const std::string &dac() const { return m_dac; };
    bool enabled() const { return m_enabled; };
    void increment_stop() { m_stop += 1; };
};

//TODO! Can we uniform enums between the libraries?

/**
 * @brief Enum class to identify different detectors. 
 * The values are the same as in slsDetectorPackage
 * Different spelling to avoid confusion with the slsDetectorPackage
 */
enum class DetectorType {
    //Standard detectors match the enum values from slsDetectorPackage
    Generic,
    Eiger,
    Gotthard,
    Jungfrau,
    ChipTestBoard,
    Moench,
    Mythen3,
    Gotthard2,
    Xilinx_ChipTestBoard,

    //Additional detectors used for defining processing. Variants of the standard ones.
    Moench03=100,
    Moench03_old,
    Unknown
};

enum class TimingMode { Auto, Trigger };
enum class FrameDiscardPolicy { NoDiscard, Discard, DiscardPartial };
enum class BurstMode { Burst_Interal, Burst_External, Continuous_Internal,
                       Continuous_External };


std::string RemoveUnit(std::string &str);

/** Convert std::chrono::duration with specified output unit */
template <typename T, typename Rep = double>
typename std::enable_if<is_duration<T>::value, std::string>::type
ToString(T t, const std::string &unit) {
    using std::chrono::duration;
    using std::chrono::duration_cast;
    std::ostringstream os;
    if (unit == "ns")
        os << duration_cast<duration<Rep, std::nano>>(t).count() << unit;
    else if (unit == "us")
        os << duration_cast<duration<Rep, std::micro>>(t).count() << unit;
    else if (unit == "ms")
        os << duration_cast<duration<Rep, std::milli>>(t).count() << unit;
    else if (unit == "s")
        os << duration_cast<duration<Rep>>(t).count() << unit;
    else
        throw std::runtime_error("Unknown unit: " + unit);
    return os.str();
}

/** Convert std::chrono::duration automatically selecting the unit */
template <typename From>
typename std::enable_if<is_duration<From>::value, std::string>::type
ToString(From t) {

    using std::chrono::duration_cast;
    using std::chrono::abs;
    using std::chrono::nanoseconds;
    using std::chrono::microseconds;
    using std::chrono::milliseconds;
    auto tns = duration_cast<nanoseconds>(t);
    if (abs(tns) <microseconds(1)) {
        return ToString(tns, "ns");
    } else if (abs(tns) < milliseconds(1)) {
        return ToString(tns, "us");
    } else if (abs(tns) < milliseconds(99)) {
        return ToString(tns, "ms");
    } else {
        return ToString(tns, "s");
    }
}

template <typename T>
T StringTo(const std::string &t, const std::string &unit) {
    double tval{0};
    try {
        tval = std::stod(t);
    } catch (const std::invalid_argument &e) {
        throw std::invalid_argument("[ERROR] Could not convert string to time");
    }

    using std::chrono::duration;
    using std::chrono::duration_cast;
    if (unit == "ns") {
        return duration_cast<T>(duration<double, std::nano>(tval));
    } else if (unit == "us") {
        return duration_cast<T>(duration<double, std::micro>(tval));
    } else if (unit == "ms") {
        return duration_cast<T>(duration<double, std::milli>(tval));
    } else if (unit == "s" || unit.empty()) {
        return duration_cast<T>(std::chrono::duration<double>(tval));
    } else {
        throw std::invalid_argument("[ERROR] Invalid unit in conversion from string to std::chrono::duration");
    }
}

template <typename T, std::enable_if_t<is_duration<T>::value, int> = 0 > 
T StringTo(const std::string &t) {
    std::string tmp{t};
    auto unit = RemoveUnit(tmp);
    return StringTo<T>(tmp, unit);
}

template <typename T, std::enable_if_t<!is_duration<T>::value, int> = 0 > 
T StringTo(const std::string &arg) { return T(arg); }

template <class T, typename = std::enable_if_t<!is_duration<T>::value>> 
std::string ToString(T arg) { return T(arg); }

template <> DetectorType StringTo(const std::string & /*name*/);
template <> std::string ToString(DetectorType arg);

template <> TimingMode StringTo(const std::string & /*mode*/);
template <> std::string ToString(TimingMode arg);

template <> FrameDiscardPolicy StringTo(const std::string & /*mode*/);
template <> std::string ToString(FrameDiscardPolicy arg);

template <> BurstMode StringTo(const std::string & /*mode*/);
template <> std::string ToString(BurstMode arg);

std::ostream &operator<<(std::ostream &os,
                         const ScanParameters &r);
template <> std::string ToString(ScanParameters arg);

std::ostream &operator<<(std::ostream &os, const ROI &roi);
template <> std::string ToString(ROI arg);


using DataTypeVariants = std::variant<uint16_t, uint32_t>;

template <typename T>
std::ostream &operator<<(std::ostream &os, const std::vector<T> &vec) {
    os << "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        os << vec[i];
        if (i != vec.size() - 1)
            os << ", ";
    }
    os << "]";
    return os;
}

template <typename T>
std::ostream &operator<<(std::ostream &os, const std::optional<T> &opt) {
    if (opt)
        os << *opt;
    else
        os << "nullopt";
    return os;
}


template <class T>
std::string ToString(const std::optional<T>& opt)
{
    return opt ? ToString(*opt) : "nullopt";
}





} // namespace aare