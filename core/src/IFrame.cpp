#include "aare/IFrame.hpp"
#include <cstddef>
#include <cstdint>
#include <iostream>

template < typename DataType>

DataType IFrame<DataType>::get(int row, int col) {
    if (row < 0 || row >= m_rows || col < 0 || col >= m_cols) {
        std::cerr << "Invalid row or column index" << std::endl;
        return 0;
    }
    return m_data[row * m_cols + col];
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

template class IFrame<uint8_t>;
template class IFrame<uint16_t>;
template class IFrame<uint32_t>;


