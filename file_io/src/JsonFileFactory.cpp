#include "aare/JsonFileFactory.hpp"
#include "aare/JsonFile.hpp"
#include "aare/SubFile.hpp"
#include "aare/defs.hpp"
#include "aare/helpers.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

template <DetectorType detector,typename DataType>
JsonFileFactory<detector,DataType>::JsonFileFactory(std::filesystem::path fpath) {
    if (not is_master_file(fpath))
        throw std::runtime_error("Json file is not a master file");
    this->fpath = fpath;
}

template <DetectorType detector,typename DataType>
void JsonFileFactory<detector,DataType>::parse_metadata(File<detector,DataType> *file) {
    std::cout << "Parsing metadata" << std::endl;
    std::ifstream ifs(file->master_fname());
    json j;
    ifs >> j;
    double v = j["Version"];
    std::cout << "Version: " << v << std::endl;
    file->version = fmt::format("{:.1f}", v);
    file->type = StringTo<DetectorType>(j["Detector Type"].get<std::string>());
    if (file->type != detector)
        throw std::runtime_error("Detector type mismatch: file type (" + toString<DetectorType>(file->type) +
                                 ") != specified type (" + toString<DetectorType>(detector) + ")");
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
    if (file->bitdepth != sizeof(DataType) * 8)
        throw std::runtime_error("Bitdepth mismatch: file bitdepth (" + std::to_string(file->bitdepth) +
                                 ") != specified bitdepth (" + std::to_string(sizeof(DataType) * 8) + ")");

    // only Eiger had quad
    if (file->type == DetectorType::Eiger) {
        file->quad = (j["Quad"] == 1);
    }
}

template <DetectorType detector,typename DataType>
void JsonFileFactory<detector,DataType>::open_subfiles(File<detector,DataType> *file) {
    for (int i = 0; i != file->n_subfiles; ++i) {

        file->subfiles.push_back(
            new SubFile(file->data_fname(i, 0), file->type, file->subfile_rows, file->subfile_cols, file->bitdepth));
    }
}

template <DetectorType detector,typename DataType>
File<detector,DataType> *JsonFileFactory<detector,DataType>::load_file() {
    std::cout << "Loading json file" << std::endl;
    JsonFile<detector,DataType> *file = new JsonFile<detector,DataType>();
    file->fname = this->fpath;
    this->parse_fname(file);
    this->parse_metadata(file);
    file->find_number_of_subfiles();
    this->find_geometry(file);
    this->open_subfiles(file);

    return file;
}

template class JsonFileFactory<DetectorType::Jungfrau, uint16_t>;