#pragma once
#include "aare/File.hpp"
#include "aare/Frame.hpp"
#include "aare/defs.hpp"
template <DetectorType detector, typename DataType>
class JsonFile : public File<detector, DataType> {

    Frame<DataType> *get_frame(int frame_number);
};