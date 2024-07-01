#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <typeinfo>

namespace aare {

// The format descriptor is a single character that specifies the type of the data
// - python documentation: https://docs.python.org/3/c-api/arg.html#numbers
// - py::format_descriptor<T>::format() (in pybind11) does not return the same format as
//   written in python.org documentation.
// - numpy also doesn't use the same format. and also numpy associates the format
//   with variable bitdepth types. (e.g. long is int64 on linux64 and int32 on win64)
//   https://numpy.org/doc/stable/reference/arrays.scalars.html
//
// github issue discussing this:
// https://github.com/pybind/pybind11/issues/1908#issuecomment-658358767
//
// [IN LINUX] the difference is for int64 (long) and uint64 (unsigned long). The format
// descriptor is 'q' and 'Q' respectively and in the documentation it is 'l' and 'k'.

// in practice numpy doesn't seem to care when reading buffer info: the library
// interprets 'q' or 'l' as int64 and 'Q' or 'L' as uint64.
// for this reason we decided to use the same format descriptor as pybind to avoid
// any further discrepancies.

// in the following order:
// int8, uint8, int16, uint16, int32, uint32, int64, uint64, float, double
const char DTYPE_FORMAT_DSC[] = {'b', 'B', 'h', 'H', 'i', 'I', 'q', 'Q', 'f', 'd'};

// on linux64 & apple
const char NUMPY_FORMAT_DSC[] = {'b', 'B', 'h', 'H', 'i', 'I', 'l', 'L', 'f', 'd'};
/**
 * @brief enum class to define the endianess of the system
 */
enum class endian {
#ifdef _WIN32
    little = 0,
    big = 1,
    native = little
#else
    little = __ORDER_LITTLE_ENDIAN__,
    big = __ORDER_BIG_ENDIAN__,
    native = __BYTE_ORDER__
#endif
};

/**
 * @brief class to define the data type of the pixels
 * @note only native endianess is supported
 */
class Dtype {
  public:
    enum TypeIndex { INT8, UINT8, INT16, UINT16, INT32, UINT32, INT64, UINT64, FLOAT, DOUBLE, ERROR, NONE };

    uint8_t bitdepth() const;
    size_t bytes() const;
    std::string format_descr() const { return std::string(1, DTYPE_FORMAT_DSC[static_cast<int>(m_type)]); }
    std::string numpy_descr() const { return std::string(1, NUMPY_FORMAT_DSC[static_cast<int>(m_type)]); }

    explicit Dtype(const std::type_info &t);
    explicit Dtype(std::string_view sv);
    static Dtype from_bitdepth(uint8_t bitdepth);

    // not explicit to allow conversions form enum to DType
    Dtype(Dtype::TypeIndex ti); // NOLINT

    bool operator==(const Dtype &other) const noexcept;
    bool operator!=(const Dtype &other) const noexcept;
    bool operator==(const std::type_info &t) const;
    bool operator!=(const std::type_info &t) const;

    // bool operator==(DType::TypeIndex ti) const;
    // bool operator!=(DType::TypeIndex ti) const;
    std::string to_string() const;
    void set_type(Dtype::TypeIndex ti) { m_type = ti; }

  private:
    TypeIndex m_type{TypeIndex::ERROR};
};

} // namespace aare