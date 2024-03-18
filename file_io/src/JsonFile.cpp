#include "aare/JsonFile.hpp"

template <DetectorType detector, typename DataType>
Frame<DataType> *JsonFile<detector, DataType>::get_frame(size_t frame_number) {

    if (frame_number > this->total_frames) {
        throw std::runtime_error("Frame number out of range");
    }
    int subfile_id = frame_number / this->max_frames_per_file;
    auto f = new Frame<DataType>(this->rows, this->cols);
    std::byte* buffer = reinterpret_cast<std::byte*>(f->_get_data());
    this->subfiles[subfile_id]->get_frame(buffer, frame_number % this->max_frames_per_file);
    return f;
}

template <DetectorType detector, typename DataType>
JsonFile<detector,DataType>::~JsonFile<detector,DataType>() {
    for (auto& subfile : subfiles) {
        delete subfile;
    }
}


template class JsonFile<DetectorType::Jungfrau, uint16_t>;