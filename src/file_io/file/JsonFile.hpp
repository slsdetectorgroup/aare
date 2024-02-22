#pragma once
#include "Frame.hpp"
#include "defs.hpp"
#include "File.hpp"

class JsonFile: public File
{
    Frame<uint16_t> get_frame(int frame_number);
    
};