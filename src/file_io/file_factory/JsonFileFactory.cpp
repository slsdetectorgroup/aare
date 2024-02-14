#include "JsonFileFactory.hpp"
#include "JsonFile.hpp"
#include "helpers.hpp"
#include <iostream>
#include <fstream>

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
    file.type = StringTo<DetectorType>(j["Detector Type"].get<std::string>());
    file.timing_mode = StringTo<TimingMode>(j["Timing Mode"].get<std::string>());
    file.total_frames = j["Frames in File"];
    file.subfile_cols = j["Pixels"]["x"];
    file.subfile_rows = j["Pixels"]["y"];
    if (type_ == DetectorType::Moench)
        bitdepth_ = 16;
    else
        bitdepth_ = j["Dynamic Range"];

    // only Eiger had quad
    if (type_ == DetectorType::Eiger) {
        quad_ = (j["Quad"] == 1);
    }




}

File JsonFileFactory::loadFile(){
    std::cout<<"Loading json file"<<std::endl;
    JsonFile file = JsonFile();
    this->parse_fname(file);
    this->parse_metadata(file);




    return file;
}