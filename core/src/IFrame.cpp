#include "aare/IFrame.hpp"
#include <cstddef>
#include <cstdint>
#include <iostream>

template < typename DataType>

DataType IFrame<DataType>::get(int row, int col) {
    if (row < 0 || row >= m_rows || col < 0 || col >= m_cols) {
        throw std::runtime_error("Invalid row or column index");
    }
    if (m_data == nullptr) {
        throw std::runtime_error("Data is not initialized (m_data == nullptr)");
    }
    return m_data[row * m_cols + col];
};

template < typename DataType>
DataType IFrame<DataType>::set(int row, int col, DataType value) {
    if (row < 0 || row >= m_rows || col < 0 || col >= m_cols) {
        throw std::runtime_error("Invalid row or column index");
    }
    if (m_data == nullptr) {
        throw std::runtime_error("Data is not initialized (m_data == nullptr)");
    }
    m_data[row * m_cols + col] = value;
    return value;
};

template < typename DataType> std::vector<std::vector<DataType>> IFrame< DataType>::get_array() {
    std::vector<std::vector<DataType>> array;
    for (int i = 0; i < m_rows; i++) {
        std::vector<DataType> row;
        row.assign(m_data + i * m_cols, m_data + (i + 1) * m_cols);
        array.push_back(row);
    }

    return array;
}
template < typename DataType>
bool IFrame<DataType>::operator==( IFrame<DataType> &rhs)
{
    if (this->rows() != rhs.rows() || this->cols() != rhs.cols() || this->bitdepth() != rhs.bitdepth())
        return false;
    if (this->_get_data() == nullptr && rhs._get_data() == nullptr)
        return true;
    if (this->_get_data() == nullptr || rhs._get_data() == nullptr)
        return false;
    if (std::memcmp(this->_get_data(), rhs._get_data(), this->rows() * this->cols() * sizeof(DataType)) != 0)
        return false;
    return true;
}
template class IFrame<uint8_t>;
template class IFrame<uint16_t>;
template class IFrame<uint32_t>;


