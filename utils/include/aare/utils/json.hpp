#pragma once
#include <array>
#include <map>
#include <string>
#include <vector>

// helper functions to write json
// append to string for better performance (not tested)

namespace aare {

/**
 * @brief write a digit to a string
 * takes key and value and outputs->"key": value,
 * @tparam T type of value (int, uint32_t, ...)
 * @param s string to append to
 * @param key key to write
 * @param value value to write
 * @return void
 * @note
 * - can't use concepts here because we are using c++17
 */
template <typename T> inline void write_digit(std::string &s, const std::string &key, const T &value) {
    s += "\"";
    s += key;
    s += "\": ";
    s += std::to_string(value);
    s += ", ";
}
inline void write_str(std::string &s, const std::string &key, const std::string &value) {
    s += "\"";
    s += key;
    s += "\": \"";
    s += value;
    s += "\", ";
}
inline void write_map(std::string &s, const std::string &key, const std::map<std::string, std::string> &value) {
    s += "\"";
    s += key;
    s += "\": {";
    for (const auto &kv : value) {
        write_str(s, kv.first, kv.second);
    }
    // remove last comma or trailing spaces
    for (size_t i = s.size() - 1; i > 0; i--) {
        if (s[i] == ',' or s[i] == ' ') {
            s.pop_back();
        } else
            break;
    }
    s += "}, ";
}
inline void write_array(std::string &s, const std::string &key, const std::array<int, 4> &value) {
    s += "\"";
    s += key;
    s += "\": [";
    s += std::to_string(value[0]);
    s += ", ";
    s += std::to_string(value[1]);
    s += ", ";
    s += std::to_string(value[2]);
    s += ", ";
    s += std::to_string(value[3]);
    s += "], ";
}

} // namespace aare
