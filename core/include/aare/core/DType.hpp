#pragma once
#include <cstdint>
#include <string>
#include <typeinfo>

namespace aare {

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

class DType {
    // TODO! support for non native endianess?
    static_assert(sizeof(long) == sizeof(int64_t), "long should be 64bits");

  public:
    enum TypeIndex { INT8, UINT8, INT16, UINT16, INT32, UINT32, INT64, UINT64, FLOAT, DOUBLE, ERROR };

    uint8_t bitdepth() const;

    explicit DType(const std::type_info &t);
    explicit DType(std::string_view sv);

    // not explicit to allow conversions form enum to DType
    DType(DType::TypeIndex ti);

    bool operator==(const DType &other) const noexcept;
    bool operator!=(const DType &other) const noexcept;
    bool operator==(const std::type_info &t) const;
    bool operator!=(const std::type_info &t) const;

    // bool operator==(DType::TypeIndex ti) const;
    // bool operator!=(DType::TypeIndex ti) const;
    std::string str() const;

  private:
    TypeIndex m_type{TypeIndex::ERROR};
};

} // namespace aare