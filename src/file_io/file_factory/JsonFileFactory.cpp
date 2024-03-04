#include "JsonFileFactory.hpp"
#include "JsonFile.hpp"
#include "SubFile.hpp"
#include "defs.hpp"
#include "helpers.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

JsonFileFactory::JsonFileFactory(std::filesystem::path fpath, uint16_t bitdepth=16) {
    if (not is_master_file(fpath))
        throw std::runtime_error("Json file is not a master file");
    this->fpath = fpath;
    this->bitdepth = bitdepth;
}

void JsonFileFactory::parse_metadata(File *file) {
    std::cout << "Parsing metadata" << std::endl;
    std::ifstream ifs(file->master_fname());
    json j;
    ifs >> j;
    double v = j["Version"];
    std::cout << "Version: " << v << std::endl;
    file->version = fmt::format("{:.1f}", v);
    file->type = StringTo<DetectorType>(j["Detector Type"].get<std::string>());
    file->timing_mode = StringTo<TimingMode>(j["Timing Mode"].get<std::string>());
    file->total_frames = j["Frames in File"];
    file->subfile_cols = j["Pixels"]["x"];
    file->subfile_rows = j["Pixels"]["y"];
    file->max_frames_per_file = j["Max Frames Per File"];
    try {
        file->bitdepth = j.at("Dynamic Range");
    } catch (const json::out_of_range &e) {
        std::cerr << "master file does not have Dynamic Range. Defaulting to 16 bit" << '\n';
        file->bitdepth = 16;
    }
    // if (file->bitdepth != this->bitdepth)
    //     throw std::runtime_error("Bitdepth mismatch: file bitdepth (" + std::to_string(file->bitdepth) +
    //                              ") != specified bitdepth (" + std::to_string(this->bitdepth) + ")");

    // only Eiger had quad
    if (file->type == DetectorType::Eiger) {
        file->quad = (j["Quad"] == 1);
    }
}

void JsonFileFactory::open_subfiles(File *file) {
    for (int i = 0; i != file->n_subfiles; ++i) {

        file->subfiles.push_back(
            new SubFile(file->data_fname(i, 0), file->type, file->subfile_rows, file->subfile_cols, file->bitdepth));
    }
}

File *JsonFileFactory::load_file() {
    std::cout << "Loading json file" << std::endl;
    JsonFile *file = new JsonFile();
    file->fname = fpath;
    this->parse_fname(file);
    this->parse_metadata(file);
    file->find_number_of_subfiles();
    this->find_geometry(file);
    this->open_subfiles(file);

    return file;
}