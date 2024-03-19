#include "aare/IFrame.hpp"
#include <iostream>
#include <sys/types.h>

template <class DataType> class DataSpan :public IFrame<DataType> {
  public:
    DataSpan(IFrame<DataType>& frame){
        this->m_data = frame._get_data();
        this->m_rows = frame.rows();
        this->m_cols = frame.cols();
    }
    DataSpan& operator=(IFrame<DataType>& frame){
        this->m_data = frame._get_data();
        this->m_rows = frame.rows();
        this->m_cols = frame.cols();
    }
    DataSpan(ssize_t rows, ssize_t cols) = delete;
    DataSpan(std::byte *fp, ssize_t rows, ssize_t cols) {
        this->m_rows = rows;
        this->m_cols = cols;
        this->m_data = reinterpret_cast<DataType *>(fp);
    }
    ~DataSpan() {}
};