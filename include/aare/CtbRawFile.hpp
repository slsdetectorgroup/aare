#pragma once

#include "aare/FileInterface.hpp"
#include "aare/file_utils.hpp"
#include "aare/Frame.hpp"

#include <filesystem>

namespace aare{

class CtbRawFile{
    FileNameComponents m_fnc{};
public:
    CtbRawFile(const std::filesystem::path &fname);



};

}