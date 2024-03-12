#pragma once
#include "aare/File.hpp"
#include "aare/defs.hpp"
#include <iostream>
#include <numeric>

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
template <DetectorType detector, typename DataType> class NumpyFile : public File<detector, DataType> {
    FILE *fp = nullptr;
    
  public:
    NumpyFile(std::filesystem::path fname);
    Frame<DataType> *get_frame(int frame_number) override;
    header_t header{};
    static constexpr std::array<char, 6> magic_str{'\x93', 'N', 'U', 'M', 'P', 'Y'};
    uint8_t major_ver_{};
    uint8_t minor_ver_{};
    uint32_t header_len{};
    uint8_t header_len_size{};
    const uint8_t magic_string_length{6};
    ssize_t header_size{};
    inline ssize_t pixels_per_frame() {
        return std::accumulate(header.shape.begin() + 1, header.shape.end(), 1, std::multiplies<uint64_t>());
    };
    inline ssize_t bytes_per_frame() { return header.dtype.itemsize * pixels_per_frame(); };
};