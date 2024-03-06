#pragma once
#include "file_io/File.hpp"
#include "core/Frame.hpp"
#include "common/defs.hpp"
template <DetectorType detector, typename DataType>
class JsonFile : public File<detector, DataType> {

    Frame<DataType> *get_frame(int frame_number);
};