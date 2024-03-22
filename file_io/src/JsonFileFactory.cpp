#include "aare/JsonFileFactory.hpp"
#include "aare/JsonFile.hpp"
#include "aare/SubFile.hpp"
#include "aare/defs.hpp"
#include "aare/helpers.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

JsonFileFactory::JsonFileFactory(std::filesystem::path fpath) {
    if (not is_master_file(fpath))
        throw std::runtime_error("Json file is not a master file");
    this->m_fpath = fpath;
}

void JsonFileFactory::parse_metadata(File*_file) {
    auto file = dynamic_cast<JsonFile*>(_file);
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
        file->bitdepth = 16;
    }
    // only Eiger had quad
    if (file->type == DetectorType::Eiger) {
        file->quad = (j["Quad"] == 1);
    }
}

void JsonFileFactory::open_subfiles(File*_file) {
    auto file = dynamic_cast<JsonFile*>(_file);
    for (int i = 0; i != file->n_subfiles; ++i) {

        file->subfiles.push_back(
            new SubFile(file->data_fname(i, 0), file->type, file->subfile_rows, file->subfile_cols, file->bitdepth));
    }
}

JsonFile* JsonFileFactory::load_file() {
    JsonFile* file = new JsonFile();
    file->fname = this->m_fpath;
    this->parse_fname(file);
    this->parse_metadata(file);
    file->find_number_of_subfiles();
    this->find_geometry(file);
    this->open_subfiles(file);
    return file;
}


sls_detector_header JsonFileFactory::read_header(const std::filesystem::path &fname) {
    sls_detector_header h{};
    FILE *fp = fopen(fname.c_str(), "r");
    if (!fp)
        throw std::runtime_error(fmt::format("Could not open: {} for reading", fname.c_str()));

    size_t rc = fread(reinterpret_cast<char *>(&h), sizeof(h), 1, fp);
    fclose(fp);
    if (rc != 1)
        throw std::runtime_error("Could not read header from file");
    return h;
}


void JsonFileFactory::find_geometry(File* _file) {
    auto file = dynamic_cast<JsonFile*>(_file);
    uint16_t r{};
    uint16_t c{};
    for (int i = 0; i != file->n_subfiles; ++i) {
        auto h = this->read_header(file->data_fname(i, 0));
        r = std::max(r, h.row);
        c = std::max(c, h.column);

        file->positions.push_back({h.row, h.column});
    }
    r++;
    c++;

    file->rows = r * file->subfile_rows;
    file->cols = c * file->subfile_cols;

    file->rows += (r - 1) * file->cfg.module_gap_row;
}

void JsonFileFactory::parse_fname(File* file) {

    file->base_path = this->m_fpath.parent_path();
    file->base_name = this->m_fpath.stem();
    file->ext = this->m_fpath.extension();

    auto pos = file->base_name.rfind("_");
    file->findex = std::stoi(file->base_name.substr(pos + 1));
    pos = file->base_name.find("_master_");
    file->base_name.erase(pos);
}


