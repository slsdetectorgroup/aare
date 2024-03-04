#include "File.hpp"
template <DetectorType detector, typename DataType>
File<detector,DataType>::~File<detector,DataType>() {
    for (auto& subfile : subfiles) {
        delete subfile;
    }
}

template class File<DetectorType::Jungfrau, uint16_t>;
