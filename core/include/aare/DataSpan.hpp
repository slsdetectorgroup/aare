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
        this->rows = rows;
        this->cols = cols;
        if (sizeof(DataType) * rows * cols != sizeof(fp)) {
            std::cerr << "Invalid data size" << std::endl;
            return;
        }
        this->m_data = reinterpret_cast<DataType *>(fp);
    }
    ~DataSpan() {}
};