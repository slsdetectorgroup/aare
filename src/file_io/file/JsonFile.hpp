#pragma once
#include "Frame.hpp"
#include "defs.hpp"
#include "File.hpp"

class JsonFile: public File
{
    template <class T>
    Frame<T> get_frame(int frame_number);

    IFrame get_frame(int frame_number);
    
};