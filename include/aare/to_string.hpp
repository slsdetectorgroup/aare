#pragma once

#include "aare/defs.hpp"
#include "aare/string_utils.hpp"

#include <optional>
#include <chrono>


namespace aare {

// generic
template <class T, typename = std::enable_if_t<!is_duration<T>::value>>
std::string ToString(T arg) {
    return T(arg);
}

template <typename T,
          std::enable_if_t<!is_duration<T>::value && !is_container<T>::value,
                           int> = 0>
T StringTo(const std::string &arg) {
    return T(arg);
}

// time

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

    using std::chrono::abs;
    using std::chrono::duration_cast;
    using std::chrono::microseconds;
    using std::chrono::milliseconds;
    using std::chrono::nanoseconds;
    auto tns = duration_cast<nanoseconds>(t);
    if (abs(tns) < microseconds(1)) {
        return ToString(tns, "ns");
    } else if (abs(tns) < milliseconds(1)) {
        return ToString(tns, "us");
    } else if (abs(tns) < milliseconds(99)) {
        return ToString(tns, "ms");
    } else {
        return ToString(tns, "s");
    }
}
template <class Rep, class Period>
std::ostream &operator<<(std::ostream &os,
                         const std::chrono::duration<Rep, Period> &d) {
    return os << ToString(d);
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
        throw std::invalid_argument("[ERROR] Invalid unit in conversion from "
                                    "string to std::chrono::duration");
    }
}

template <typename T, std::enable_if_t<is_duration<T>::value, int> = 0>
T StringTo(const std::string &t) {
    std::string tmp{t};
    auto unit = RemoveUnit(tmp);
    return StringTo<T>(tmp, unit);
}

template <> inline bool StringTo(const std::string &s) {
    int i = std::stoi(s, nullptr, 10);
    switch (i) {
    case 0:
        return false;
    case 1:
        return true;
    default:
        throw std::runtime_error("Unknown boolean. Expecting be 0 or 1.");
    }
}

template <> inline uint8_t StringTo(const std::string &s) {
    int base = s.find("0x") != std::string::npos ? 16 : 10;
    int value = std::stoi(s, nullptr, base);
    if (value < std::numeric_limits<uint8_t>::min() ||
        value > std::numeric_limits<uint8_t>::max()) {
        throw std::runtime_error("Cannot scan uint8_t from string '" + s +
                                 "'. Value must be in range 0 - 255.");
    }
    return static_cast<uint8_t>(value);
}

template <> inline uint16_t StringTo(const std::string &s) {
    int base = s.find("0x") != std::string::npos ? 16 : 10;
    int value = std::stoi(s, nullptr, base);
    if (value < std::numeric_limits<uint16_t>::min() ||
        value > std::numeric_limits<uint16_t>::max()) {
        throw std::runtime_error("Cannot scan uint16_t from string '" + s +
                                 "'. Value must be in range 0 - 65535.");
    }
    return static_cast<uint16_t>(value);
}

template <> inline uint32_t StringTo(const std::string &s) {
    int base = s.find("0x") != std::string::npos ? 16 : 10;
    return std::stoul(s, nullptr, base);
}

template <> inline uint64_t StringTo(const std::string &s) {
    int base = s.find("0x") != std::string::npos ? 16 : 10;
    return std::stoull(s, nullptr, base);
}

template <> inline int StringTo(const std::string &s) {
    int base = s.find("0x") != std::string::npos ? 16 : 10;
    return std::stoi(s, nullptr, base);
}

/*template <> inline size_t StringTo(const std::string &s) {
    int base = s.find("0x") != std::string::npos ? 16 : 10;
    return std::stoull(s, nullptr, base);
}*/

// vector
template <typename T> std::string ToString(const std::vector<T> &vec) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        oss << vec[i];
        if (i != vec.size() - 1)
            oss << ", ";
    }
    oss << "]";
    return oss.str();
}

template <typename T>
std::ostream &operator<<(std::ostream &os, const std::vector<T> &v) {
    return os << ToString(v);
}

template <typename Container,
          std::enable_if_t<is_container<Container>::value &&
                               !is_std_string_v<Container> /*&&
                               !is_map_v<Container>*/
                           ,
                           int> = 0>
Container StringTo(const std::string &s) {
    using Value = typename Container::value_type;

    // strip outer brackets
    std::string str = s;
    str.erase(
        std::remove_if(str.begin(), str.end(),
                       [](unsigned char c) { return c == '[' || c == ']'; }),
        str.end());

    std::stringstream ss(str);
    std::string item;
    Container result;

    while (std::getline(ss, item, ',')) {
        TrimWhiteSpaces(item);
        if (!item.empty()) {
            result.push_back(StringTo<Value>(item));
        }
    }
    return result;
}

// map
template <typename KeyType, typename ValueType>
std::string ToString(const std::map<KeyType, ValueType> &m) {
    std::ostringstream os;
    os << '{';
    if (!m.empty()) {
        auto it = m.cbegin();
        os << ToString(it->first) << ": " << ToString(it->second);
        it++;
        while (it != m.cend()) {
            os << ", " << ToString(it->first) << ": " << ToString(it->second);
            it++;
        }
    }
    os << '}';
    return os.str();
}

template <>
inline std::map<std::string, std::string> StringTo(const std::string &s) {
    std::map<std::string, std::string> result;
    std::string str = s;

    // Remove outer braces if present
    if (!str.empty() && str.front() == '{' && str.back() == '}') {
        str = str.substr(1, str.size() - 2);
    }

    std::stringstream ss(str);
    std::string item;

    while (std::getline(ss, item, ',')) {
        auto colon_pos = item.find(':');
        if (colon_pos == std::string::npos)
            throw std::runtime_error("Missing ':' in item: " + item);

        std::string key = item.substr(0, colon_pos);
        std::string value = item.substr(colon_pos + 1);

        TrimWhiteSpaces(key);
        TrimWhiteSpaces(value);

        result[key] = value;
    }
    return result;
}

// optional
template <class T> std::string ToString(const std::optional<T> &opt) {
    return opt ? ToString(*opt) : "nullopt";
}

template <typename T>
std::ostream &operator<<(std::ostream &os, const std::optional<T> &opt) {
    if (opt)
        os << *opt;
    else
        os << "nullopt";
    return os;
}

// enums
template <> DetectorType StringTo(const std::string & /*name*/);
template <> std::string ToString(DetectorType arg);

template <> TimingMode StringTo(const std::string & /*mode*/);
template <> std::string ToString(TimingMode arg);

template <> FrameDiscardPolicy StringTo(const std::string & /*mode*/);
template <> std::string ToString(FrameDiscardPolicy arg);

template <> BurstMode StringTo(const std::string & /*mode*/);
template <> std::string ToString(BurstMode arg);

std::ostream &operator<<(std::ostream &os, const ScanParameters &r);
template <> std::string ToString(ScanParameters arg);

std::ostream &operator<<(std::ostream &os, const ROI &roi);
template <> std::string ToString(ROI arg);

} // namespace aare