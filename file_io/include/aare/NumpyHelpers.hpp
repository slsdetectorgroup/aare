
#pragma once
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <numeric>
#include <iostream>

#include "aare/defs.hpp"

using shape_t = std::vector<uint64_t>;

struct dtype_t {
    char byteorder;
    char kind;
    unsigned int itemsize;
    std::string to_string() {
        std::stringstream sstm;
        sstm << byteorder << kind << itemsize;
        return sstm.str();
    }
};
struct header_t {
    dtype_t dtype;
    bool fortran_order;
    shape_t shape;
    std::string to_string() {
        std::stringstream sstm;
        sstm << "dtype: " << dtype.to_string() << ", fortran_order: " << fortran_order << ' ';

        sstm << "shape: (";
        for (auto item : shape)
            sstm << item << ',';
        sstm << ')';
        return sstm.str();
    }
};


std::string parse_str(const std::string &in);
/**
  Removes leading and trailing whitespaces
  */
std::string trim(const std::string &str);

std::vector<std::string> parse_tuple(std::string in);

bool parse_bool(const std::string &in);

std::string get_value_from_map(const std::string &mapstr);

std::unordered_map<std::string, std::string> parse_dict(std::string in, const std::vector<std::string> &keys);

template <typename T, size_t N> inline bool in_array(T val, const std::array<T, N> &arr) {
    return std::find(std::begin(arr), std::end(arr), val) != std::end(arr);
}
bool is_digits(const std::string &str);

dtype_t parse_descr(std::string typestring);
