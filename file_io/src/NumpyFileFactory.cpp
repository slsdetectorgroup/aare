#include "aare/NumpyFileFactory.hpp"

template <DetectorType detector, typename DataType>
NumpyFileFactory<detector, DataType>::NumpyFileFactory(std::filesystem::path fpath) {
    this->fpath = fpath;
}
inline std::string parse_str(const std::string &in) {
    if ((in.front() == '\'') && (in.back() == '\''))
        return in.substr(1, in.length() - 2);

    throw std::runtime_error("Invalid python string.");
}
/**
  Removes leading and trailing whitespaces
  */
inline std::string trim(const std::string& str) {
    const std::string whitespace = " \t";
    auto begin = str.find_first_not_of(whitespace);

    if (begin == std::string::npos)
        return "";

    auto end = str.find_last_not_of(whitespace);

    return str.substr(begin, end - begin + 1);
}
inline std::vector<std::string> parse_tuple(std::string in) {
    std::vector<std::string> v;
    const char seperator = ',';

    in = trim(in);

    if ((in.front() == '(') && (in.back() == ')'))
        in = in.substr(1, in.length() - 2);
    else
        throw std::runtime_error("Invalid Python tuple.");

    std::istringstream iss(in);

    for (std::string token; std::getline(iss, token, seperator);) {
        v.push_back(token);
    }

    return v;
}
inline bool parse_bool(const std::string &in) {
    if (in == "True")
        return true;
    if (in == "False")
        return false;

    throw std::runtime_error("Invalid python boolan.");
}


inline std::string get_value_from_map(const std::string &mapstr) {
    size_t sep_pos = mapstr.find_first_of(":");
    if (sep_pos == std::string::npos)
        return "";

    std::string tmp = mapstr.substr(sep_pos + 1);
    return trim(tmp);
}
std::unordered_map<std::string, std::string> parse_dict(std::string in, const std::vector<std::string> &keys) {
    std::unordered_map<std::string, std::string> map;

    if (keys.size() == 0)
        return map;

    in = trim(in);

    // unwrap dictionary
    if ((in.front() == '{') && (in.back() == '}'))
        in = in.substr(1, in.length() - 2);
    else
        throw std::runtime_error("Not a Python dictionary.");

    std::vector<std::pair<size_t, std::string>> positions;

    for (auto const &value : keys) {
        size_t pos = in.find("'" + value + "'");

        if (pos == std::string::npos)
            throw std::runtime_error("Missing '" + value + "' key.");

        std::pair<size_t, std::string> position_pair{pos, value};
        positions.push_back(position_pair);
    }

    // sort by position in dict
    std::sort(positions.begin(), positions.end());

    for (size_t i = 0; i < positions.size(); ++i) {
        std::string raw_value;
        size_t begin{positions[i].first};
        size_t end{std::string::npos};

        std::string key = positions[i].second;

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


using shape_t = std::vector<uint64_t>;

struct dtype_t {
    char byteorder;
    char kind;
    unsigned int itemsize;
};
struct header_t {
    dtype_t dtype;
    bool fortran_order;
    shape_t shape;
};
template <typename T, size_t N> inline bool in_array(T val, const std::array<T, N> &arr) {
    return std::find(std::begin(arr), std::end(arr), val) != std::end(arr);
}
inline bool is_digits(const std::string &str) { return std::all_of(str.begin(), str.end(), ::isdigit); }

inline dtype_t parse_descr(std::string typestring) {
    if (typestring.length() < 3) {
        throw std::runtime_error("invalid typestring (length)");
    }

    char byteorder_c = typestring.at(0);
    char kind_c = typestring.at(1);
    std::string itemsize_s = typestring.substr(2);

    if (!in_array(byteorder_c, endian_chars)) {
        throw std::runtime_error("invalid typestring (byteorder)");
    }

    if (!in_array(kind_c, numtype_chars)) {
        throw std::runtime_error("invalid typestring (kind)");
    }

    if (!is_digits(itemsize_s)) {
        throw std::runtime_error("invalid typestring (itemsize)");
    }
    unsigned int itemsize = std::stoul(itemsize_s);

    return {byteorder_c, kind_c, itemsize};
}

template <DetectorType detector, typename DataType>
void NumpyFileFactory<detector, DataType>::parse_metadata(File<detector, DataType> *_file) {
    auto file = dynamic_cast<NumpyFile<detector, DataType> *>(_file);
    // open ifsteam to file
    std::ifstream f(file->fname, std::ios::binary);
    if (!f.is_open()) {
        throw std::runtime_error(fmt::format("Could not open: {} for reading", file->fname.c_str()));
    }
    // read magic number
    std::array<char, 6> tmp{};
    f.read(tmp.data(), tmp.size());
    if (tmp != NumpyFileFactory<detector, DataType>::magic_str) {
        for (auto item : tmp)
            fmt::print("{}, ", int(item));
        fmt::print("\n");
        throw std::runtime_error("Not a numpy file");
    }

    // read version
    f.read(reinterpret_cast<char *>(&major_ver_), 1);
    f.read(reinterpret_cast<char *>(&minor_ver_), 1);

    if (major_ver_ == 1) {
        header_len_size = 2;
    } else if (major_ver_ == 2) {
        header_len_size = 4;
    } else {
        throw std::runtime_error("Unsupported numpy version");
    }
    // read header length
    f.read(reinterpret_cast<char *>(&header_len), header_len_size);
    if ((magic_string_length + 2 + header_len_size + header_len) % 16 != 0) {
        fmt::print("Warning: header length is not a multiple of 16\n");
    }
    // read header
    auto buf_v = std::vector<char>(header_len);
    f.read(buf_v.data(), header_len);
    std::string header(buf_v.data(), header_len);

    // parse header

    std::vector<std::string> keys{"descr", "fortran_order", "shape"};

    auto dict_map = parse_dict(header, keys);
    if (dict_map.size() == 0)
        throw std::runtime_error("invalid dictionary in header");

    std::string descr_s = dict_map["descr"];
    std::string fortran_s = dict_map["fortran_order"];
    std::string shape_s = dict_map["shape"];

    std::string descr = parse_str(descr_s);
    dtype_t dtype = parse_descr(descr);

    // convert literal Python bool to C++ bool
    bool fortran_order = parse_bool(fortran_s);

    // parse the shape tuple
    auto shape_v = parse_tuple(shape_s);
    shape_t shape;
    for (auto item : shape_v) {
        auto dim = static_cast<unsigned long>(std::stoul(item));
        shape.push_back(dim);
    }

    // {dtype, fortran_order, shape};
};