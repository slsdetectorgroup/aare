
#include "aare/NumpyFile.hpp"

template <DetectorType detector, typename DataType>
NumpyFile<detector, DataType>::NumpyFile(std::filesystem::path fname){
    this->fname = fname;
    fp = fopen(fname.c_str(), "rb");
}

template <DetectorType detector, typename DataType>
Frame<DataType> *NumpyFile<detector, DataType>::get_frame(int frame_number) {
    if (fp == nullptr) {
        throw std::runtime_error("File not open");
    }
    if (frame_number > header.shape[0]) {
        throw std::runtime_error("Frame number out of range");
    }
    Frame<DataType> *frame = new Frame<DataType>(header.shape[1], header.shape[2]);
    fseek(fp, header_size + frame_number * bytes_per_frame(), SEEK_SET);
    fread(frame->data, sizeof(DataType), pixels_per_frame(), fp);
    return frame;
}

template class NumpyFile<DetectorType::Jungfrau, uint16_t>;