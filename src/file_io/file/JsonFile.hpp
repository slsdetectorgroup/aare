#pragma once
#include "Frame.hpp"
#include "defs.hpp"
#include "File.hpp"

class JsonFile: public File
{

    FrameImpl* get_frame(int frame_number);
    
};