#include "aare/file_utils.hpp"

namespace aare {



bool is_master_file(const std::filesystem::path &fpath) {
    std::string const stem = fpath.stem().string();
    return stem.find("_master_") != std::string::npos;
}

FileNameComponents parse_fname(const std::filesystem::path &fname) {
    FileNameComponents fnc;
    fnc.base_path = fname.parent_path();
    fnc.base_name = fname.stem();
    fnc.ext = fname.extension();
    try {
        auto pos = fnc.base_name.rfind('_');
        fnc.findex = std::stoi(fnc.base_name.substr(pos + 1));
    } catch (const std::invalid_argument &e) {
        fnc.valid = false;
    }
    auto pos = fnc.base_name.find("_master_");
    if (pos != std::string::npos) {
        fnc.base_name.erase(pos);
    }else{
        fnc.valid = false;
    }
    fnc.valid = true;
    return fnc;
}

} // namespace aare