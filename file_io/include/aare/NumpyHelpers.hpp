
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

#include "aare/DType.hpp"
#include "aare/defs.hpp"

using shape_t = std::vector<size_t>;

struct header_t {
    header_t() : dtype(aare::DType(aare::DType::ERROR)), fortran_order(false), shape(shape_t()){};
    header_t(aare::DType dtype, bool fortran_order, shape_t shape)
        : dtype(dtype), fortran_order(fortran_order), shape(shape){};
    aare::DType dtype;
    bool fortran_order;
    shape_t shape;
    std::string to_string() {
        std::stringstream sstm;
        sstm << "dtype: " << dtype.str() << ", fortran_order: " << fortran_order << ' ';

        sstm << "shape: (";
        for (auto item : shape)
            sstm << item << ',';
        sstm << ')';
        return sstm.str();
    }
};

namespace aare::NumpyHelpers {
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

aare::DType parse_descr(std::string typestring);
size_t write_header(std::filesystem::path fname, const header_t &header) ;
size_t write_header(std::ostream &out, const header_t &header) ;
bool is_digits(const std::string &str);

} // namespace aare::NumpyHelpers