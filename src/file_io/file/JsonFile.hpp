#pragma once
#include "File.hpp"
#include "Frame.hpp"
#include "defs.hpp"
template <DetectorType detector, typename DataType>
class JsonFile : public File<detector, DataType> {

    Frame<DataType> *get_frame(int frame_number);
};