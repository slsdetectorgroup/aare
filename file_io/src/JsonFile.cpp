#include "aare/JsonFile.hpp"
#include <typeinfo>

template <DetectorType detector, typename DataType>
Frame<DataType> *JsonFile<detector, DataType>::get_frame(int frame_number) {
    int subfile_id = frame_number / this->max_frames_per_file;
    std::byte *buffer;
    size_t frame_size = this->subfiles[subfile_id]->bytes_per_frame();
    buffer = new std::byte[frame_size];

    this->subfiles[subfile_id]->get_frame(buffer, frame_number % this->max_frames_per_file);

    auto f = new Frame<DataType>(buffer, this->rows, this->cols);

    delete[] buffer;
    return f;
}

template class JsonFile<DetectorType::Jungfrau, uint16_t>;
