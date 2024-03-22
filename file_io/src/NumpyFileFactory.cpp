#include "aare/NumpyFileFactory.hpp"
#include "aare/NumpyHelpers.hpp"
NumpyFileFactory::NumpyFileFactory(std::filesystem::path fpath) {
    this->m_fpath = fpath;
}
void NumpyFileFactory::parse_metadata(File *_file) {
    auto file = dynamic_cast<NumpyFile*>(_file);
    // open ifsteam to file
    f = std::ifstream(file->fname, std::ios::binary);
    // check if file exists
    if (!f.is_open()) {
        throw std::runtime_error(fmt::format("Could not open: {} for reading", file->fname.c_str()));
    }
    // read magic number
    std::array<char, 6> tmp{};
    f.read(tmp.data(), tmp.size());
    if (tmp != NumpyFile::magic_str) {
        for (auto item : tmp)
            fmt::print("{}, ", int(item));
        fmt::print("\n");
        throw std::runtime_error("Not a numpy file");
    }

    // read version
    f.read(reinterpret_cast<char *>(&file->major_ver_), 1);
    f.read(reinterpret_cast<char *>(&file->minor_ver_), 1);

    if (file->major_ver_ == 1) {
        file->header_len_size = 2;
    } else if (file->major_ver_ == 2) {
        file->header_len_size = 4;
    } else {
        throw std::runtime_error("Unsupported numpy version");
    }
    // read header length
    f.read(reinterpret_cast<char *>(&file->header_len), file->header_len_size);
    file->header_size = file->magic_string_length + 2 + file->header_len_size + file->header_len;
    if (file->header_size % 16 != 0) {
        fmt::print("Warning: header length is not a multiple of 16\n");
    }
    // read header
    auto buf_v = std::vector<char>(file->header_len);
    f.read(buf_v.data(), file->header_len);
    std::string header(buf_v.data(), file->header_len);

    // parse header

    std::vector<std::string> keys{"descr", "fortran_order", "shape"};
    std::cout << "original header: " << '"' << header << '"' << std::endl;

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
    file->header = {dtype, fortran_order, shape};
}

 NumpyFile* NumpyFileFactory::load_file() {
    NumpyFile* file = new NumpyFile(this->m_fpath);
    parse_metadata(file);
    std::cout << "parsed header: " << file->header.to_string() << std::endl;      
    return file;
};


