
#pragma once
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "aare/Dtype.hpp"
#include "aare/defs.hpp"

namespace aare {

struct NumpyHeader {
    Dtype dtype{aare::Dtype::ERROR};
    bool fortran_order{false};
    std::vector<size_t> shape{};
    std::string to_string() const;
};

namespace NumpyHelpers {

const constexpr std::array<char, 6> magic_str{'\x93', 'N', 'U', 'M', 'P', 'Y'};
const uint8_t magic_string_length{6};

std::string parse_str(const std::string &in);
/**
  Removes leading and trailing whitespaces
  */
std::string trim(const std::string &str);

std::vector<std::string> parse_tuple(std::string in);

bool parse_bool(const std::string &in);

std::string get_value_from_map(const std::string &mapstr);

std::unordered_map<std::string, std::string> parse_dict(std::string in, const std::vector<std::string> &keys);

template <typename T, size_t N> bool in_array(T val, const std::array<T, N> &arr) {
    return std::find(std::begin(arr), std::end(arr), val) != std::end(arr);
}
bool is_digits(const std::string &str);

aare::Dtype parse_descr(std::string typestring);
size_t write_header(const std::filesystem::path &fname, const NumpyHeader &header);
size_t write_header(std::ostream &out, const NumpyHeader &header);

} // namespace NumpyHelpers
} // namespace aare