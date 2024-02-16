#include "JsonFileFactory.hpp"
#include "JsonFile.hpp"
#include "helpers.hpp"
#include <iostream>
#include <fstream>
#include "defs.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;




JsonFileFactory::JsonFileFactory(std::filesystem::path fpath){
    if(not is_master_file(fpath))
        throw std::runtime_error("Json file is not a master file");
    this->fpath = fpath;
}

void JsonFileFactory::parse_metadata(File& file){
    std::cout<<"Parsing metadata"<<std::endl;
    std::ifstream ifs(file.master_fname());
    json j;
    ifs >> j;
    double v = j["Version"];
    file.version = fmt::format("{:.1f}", v);
    std::string tmp;
    j["Detector Type"].get_to(tmp);
    file.type = StringTo<DetectorType>(tmp);
    file.timing_mode = StringTo<TimingMode>(j["Timing Mode"].get<std::string>());
    file.total_frames = j["Frames in File"];
    file.subfile_cols = j["Pixels"]["x"];
    file.subfile_rows = j["Pixels"]["y"];
    if (file.type == DetectorType::Moench)
        file.bitdepth = 16;
    else
        file.bitdepth = j["Dynamic Range"];

    // only Eiger had quad
    if (file.type == DetectorType::Eiger) {
        file.quad = (j["Quad"] == 1);
    }




}

File JsonFileFactory::loadFile(){
    std::cout<<"Loading json file"<<std::endl;
    JsonFile file = JsonFile();
    file.fname = fpath;
    this->parse_fname(file);
    this->parse_metadata(file);
    file.find_number_of_subfiles();
    this->find_geometry(file);
    




    return file;
}