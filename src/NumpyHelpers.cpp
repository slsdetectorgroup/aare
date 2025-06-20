/*
   28-03-2024 modified by: Bechir Braham <bechir.braham@psi.ch>

   Copyright 2017-2023 Leon Merten Lohse

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

#include "aare/NumpyHelpers.hpp"
#include <iterator>

namespace aare {

std::string NumpyHeader::to_string() const {
    std::stringstream sstm;
    sstm << "dtype: " << dtype.to_string()
         << ", fortran_order: " << fortran_order << ' ';
    sstm << "shape: (";
    for (auto item : shape)
        sstm << item << ',';
    sstm << ')';
    return sstm.str();
}

namespace NumpyHelpers {

std::unordered_map<std::string, std::string>
parse_dict(std::string in, const std::vector<std::string> &keys) {
    std::unordered_map<std::string, std::string> map;
    if (keys.empty())
        return map;

    in = trim(in);

    // unwrap dictionary
    if ((in.front() == '{') && (in.back() == '}'))
        in = in.substr(1, in.length() - 2);
    else
        throw std::runtime_error("Not a Python dictionary.");

    std::vector<std::pair<size_t, std::string>> positions;

    for (auto const &key : keys) {
        size_t const pos = in.find("'" + key + "'");

        if (pos == std::string::npos)
            throw std::runtime_error("Missing '" + key + "' key.");

        std::pair<size_t, std::string> const position_pair{pos, key};
        positions.push_back(position_pair);
    }

    // sort by position in dict
    std::sort(positions.begin(), positions.end());

    for (size_t i = 0; i < positions.size(); ++i) {
        std::string raw_value;
        size_t const begin{positions[i].first};
        size_t end{std::string::npos};

        std::string const key = positions[i].second;

        if (i + 1 < positions.size())
            end = positions[i + 1].first;

        raw_value = in.substr(begin, end - begin);

        raw_value = trim(raw_value);

        if (raw_value.back() == ',')
            raw_value.pop_back();

        map[key] = get_value_from_map(raw_value);
    }

    return map;
}

aare::Dtype parse_descr(std::string typestring) {

    if (typestring.length() < 3) {
        throw std::runtime_error("invalid typestring (length)");
    }

    constexpr char little_endian_char = '<';
    constexpr char big_endian_char = '>';
    constexpr char no_endian_char = '|';
    constexpr std::array<char, 3> endian_chars = {
        little_endian_char, big_endian_char, no_endian_char};
    constexpr std::array<char, 4> numtype_chars = {'f', 'i', 'u', 'c'};

    const char byteorder_c = typestring[0];
    const char kind_c = typestring[1];
    std::string const itemsize_s = typestring.substr(2);

    if (!in_array(byteorder_c, endian_chars)) {
        throw std::runtime_error("invalid typestring (byteorder)");
    }

    if (!in_array(kind_c, numtype_chars)) {
        throw std::runtime_error("invalid typestring (kind)");
    }

    if (!is_digits(itemsize_s)) {
        throw std::runtime_error("invalid typestring (itemsize)");
    }

    return aare::Dtype(typestring);
}

bool parse_bool(const std::string &in) {
    if (in == "True")
        return true;
    if (in == "False")
        return false;
    throw std::runtime_error("Invalid python boolean.");
}

std::string get_value_from_map(const std::string &mapstr) {
    size_t const sep_pos = mapstr.find_first_of(':');
    if (sep_pos == std::string::npos)
        return "";

    std::string const tmp = mapstr.substr(sep_pos + 1);
    return trim(tmp);
}

bool is_digits(const std::string &str) {
    return std::all_of(str.begin(), str.end(), ::isdigit);
}

std::vector<std::string> parse_tuple(std::string in) {
    std::vector<std::string> v;
    const char separator = ',';

    in = trim(in);

    if ((in.front() == '(') && (in.back() == ')'))
        in = in.substr(1, in.length() - 2);
    else
        throw std::runtime_error("Invalid Python tuple.");

    std::istringstream iss(in);

    for (std::string token; std::getline(iss, token, separator);) {
        v.push_back(token);
    }

    return v;
}

std::string trim(const std::string &str) {
    const std::string whitespace = " \t\n";
    auto begin = str.find_first_not_of(whitespace);

    if (begin == std::string::npos)
        return "";

    auto end = str.find_last_not_of(whitespace);
    return str.substr(begin, end - begin + 1);
}

std::string parse_str(const std::string &in) {
    if ((in.front() == '\'') && (in.back() == '\''))
        return in.substr(1, in.length() - 2);

    throw std::runtime_error("Invalid python string.");
}

void write_magic(std::ostream &ostream, int version_major, int version_minor) {
    ostream.write(magic_str.data(), magic_string_length);
    ostream.put(static_cast<char>(version_major));
    ostream.put(static_cast<char>(version_minor));
}
template <typename T> inline std::string write_tuple(const std::vector<T> &v) {

    if (v.empty())
        return "()";
    std::ostringstream ss;
    ss.imbue(std::locale("C"));

    if (v.size() == 1) {
        ss << "(" << v.front() << ",)";
    } else {
        const std::string delimiter = ", ";
        // v.size() > 1
        ss << "(";
        // for (size_t i = 0; i < v.size() - 1; ++i) {
        //     ss << v[i] << delimiter;
        // }
        // ss << v.back();
        std::copy(v.begin(), v.end() - 1, std::ostream_iterator<T>(ss, ", "));
        ss << v.back();
        ss << ")";
    }

    return ss.str();
}

inline std::string write_boolean(bool b) {
    if (b)
        return "True";
    return "False";
}

inline std::string write_header_dict(const std::string &descr,
                                     bool fortran_order,
                                     const std::vector<size_t> &shape) {
    std::string const s_fortran_order = write_boolean(fortran_order);
    std::string const shape_s = write_tuple(shape);

    return "{'descr': '" + descr + "', 'fortran_order': " + s_fortran_order +
           ", 'shape': " + shape_s + ", }";
}

size_t write_header(const std::filesystem::path &fname,
                    const NumpyHeader &header) {
    std::ofstream out(fname, std::ios::binary | std::ios::out);
    return write_header(out, header);
}

size_t write_header(std::ostream &out, const NumpyHeader &header) {
    std::string const header_dict = write_header_dict(
        header.dtype.to_string(), header.fortran_order, header.shape);

    size_t length = magic_string_length + 2 + 2 + header_dict.length() + 1;

    int version_major = 1;
    int version_minor = 0;
    if (length >= static_cast<size_t>(255) * 255) {
        length = magic_string_length + 2 + 4 + header_dict.length() + 1;
        version_major = 2;
        version_minor = 0;
    }
    size_t const padding_len = 16 - length % 16;
    std::string const padding(padding_len, ' ');

    // write magic
    write_magic(out, version_major, version_minor);

    // write header length
    if (version_major == 1 && version_minor == 0) {
        auto header_len =
            static_cast<uint16_t>(header_dict.length() + padding.length() + 1);

        std::array<uint8_t, 2> header_len_le16{
            static_cast<uint8_t>((header_len >> 0) & 0xff),
            static_cast<uint8_t>((header_len >> 8) & 0xff)};
        out.write(reinterpret_cast<char *>(header_len_le16.data()), 2);
    } else {
        auto header_len =
            static_cast<uint32_t>(header_dict.length() + padding.length() + 1);

        std::array<uint8_t, 4> header_len_le32{
            static_cast<uint8_t>((header_len >> 0) & 0xff),
            static_cast<uint8_t>((header_len >> 8) & 0xff),
            static_cast<uint8_t>((header_len >> 16) & 0xff),
            static_cast<uint8_t>((header_len >> 24) & 0xff)};
        out.write(reinterpret_cast<char *>(header_len_le32.data()), 4);
    }

    out << header_dict << padding << '\n';
    return length;
}

} // namespace NumpyHelpers
} // namespace aare
