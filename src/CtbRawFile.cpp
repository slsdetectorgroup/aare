#include "aare/CtbRawFile.hpp"



namespace aare{

CtbRawFile::CtbRawFile(const std::filesystem::path &fname){

    if(!std::filesystem::exists(fname)){
        throw std::runtime_error(LOCATION + "File does not exist");
    }
    
    m_fnc = parse_fname(fname);
    if(!m_fnc.valid){
        throw std::runtime_error(LOCATION + "Could not parse master file name");
    }


}


} // namespace aare