#pragma once
#include <cstring>
#include <sys/types.h>
#include <vector>

/**
 * @brief Interface for Frame, DataSpan and ImageData classes
 * currently does not force any methods to be implemented
 */
template <class DataType> class IFrame {
  protected:
    ssize_t m_rows;
    ssize_t m_cols;
    DataType *m_data = nullptr;
    ssize_t m_bitdepth = sizeof(DataType) * 8;

  public:
    std::vector<std::vector<DataType>> get_array();
    DataType get(int row, int col);
    DataType set(int row, int col, DataType value);
    ssize_t rows() const { return m_rows; }
    ssize_t cols() const { return m_cols; }
    ssize_t bitdepth() const { return m_bitdepth; }
    DataType *_get_data() { return m_data; }
    bool operator==(IFrame<DataType> &rhs);
    /*
     * @brief Set the data pointer
     * WILL TRANSFER OWNERSHIP OF THE POINTER TO THE IFRAME
     */
    void _set_data(DataType *data) { m_data = data; }
};