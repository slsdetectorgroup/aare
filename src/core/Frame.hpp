#include <cstddef>
#include <sys/types.h>
#include <cstdint>
#include <bits/unique_ptr.h>
#include <vector>
#include "defs.hpp"

class Frame{

    ssize_t nrows{};
    ssize_t ncols{};
    uint8_t bitdepth_{};
    std::unique_ptr<std::byte[]> data_{nullptr};
public:
    ssize_t rows() const;
    ssize_t cols() const;
    image_shape shape() const;
    uint8_t bits_per_pixel() const;
    uint8_t bytes_per_pixel() const;
    size_t total_bytes() const;



};