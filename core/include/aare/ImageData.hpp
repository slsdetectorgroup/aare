#include "IFrame.hpp"
#include <iostream>
#include <cstdint>


template <typename DataType> class ImageData : public IFrame<DataType> {

  public:
    ImageData(IFrame<DataType>&);
    ImageData &operator=(IFrame<DataType>&);
    ImageData(ssize_t rows, ssize_t cols);
    ImageData(std::byte *fp, ssize_t rows, ssize_t cols);
    ~ImageData() { delete[] this->m_data; };

  private:
    void _copy(IFrame<DataType>& frame);    
};