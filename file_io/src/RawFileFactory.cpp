#include "aare/RawFileFactory.hpp"
#include "aare/RawFile.hpp"
#include "aare/SubFile.hpp"
#include "aare/defs.hpp"
#include "aare/helpers.hpp"
#include "aare/utils/logger.hpp"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

RawFileFactory::RawFileFactory(std::filesystem::path fpath) {
    if (not is_master_file(fpath))
        throw std::runtime_error("Json file is not a master file");
    this->m_fpath = fpath;
}

void RawFileFactory::parse_metadata(File *_file) {
    auto file = dynamic_cast<RawFile *>(_file);
    if (file->ext == ".raw") {
        this->parse_raw_metadata(file);
        if (file->bitdepth == 0) {
            switch (file->type) {
            case DetectorType::Eiger:
                file->bitdepth = 32;
                break;

            default:
                file->bitdepth = 16;
            }
        }
    } else if (file->ext == ".json") {
        this->parse_json_metadata(file);
    } else {
        throw std::runtime_error("Unsupported file type");
    }
    file->n_subfile_parts = file->geometry.row * file->geometry.col;
}

void RawFileFactory::parse_raw_metadata(RawFile *file) {
    std::ifstream ifs(file->master_fname());
    for (std::string line; std::getline(ifs, line);) {
        if (line == "#Frame Header")
            break;
        auto pos = line.find(":");
        auto key_pos = pos;
        while (key_pos != std::string::npos && std::isspace(line[--key_pos]))
            ;
        if (key_pos != std::string::npos) {
            auto key = line.substr(0, key_pos + 1);
            auto value = line.substr(pos + 2);
            // do the actual parsing
            if (key == "Version") {
                file->version = value;
            } else if (key == "TimeStamp") {

            } else if (key == "Detector Type") {
                file->type = StringTo<DetectorType>(value);
            } else if (key == "Timing Mode") {
                file->timing_mode = StringTo<TimingMode>(value);
            } else if (key == "Pixels") {
                // Total number of pixels cannot be found yet looking at
                // submodule
                 pos = value.find(',');
                file->subfile_cols = std::stoi(value.substr(1, pos));
                file->subfile_rows = std::stoi(value.substr(pos + 1));
            } else if (key == "Total Frames") {
                file->total_frames = std::stoi(value);
            } else if (key == "Dynamic Range") {
                file->bitdepth = std::stoi(value);
            } else if (key == "Quad") {
                file->quad = (value == "1");
            } else if (key == "Max Frames Per File") {
                file->max_frames_per_file = std::stoi(value);
            } else if (key == "Geometry") {
                 pos = value.find(',');
                file->geometry = {std::stoi(value.substr(1, pos)), std::stoi(value.substr(pos + 1))};
            }
        }
    }
}

void RawFileFactory::parse_json_metadata(RawFile *file) {
    std::ifstream ifs(file->master_fname());
    json j;
    ifs >> j;
    double v = j["Version"];
    std::cout << "Version: " << v << std::endl;
    file->version = fmt::format("{:.1f}", v);
    file->type = StringTo<DetectorType>(j["Detector Type"].get<std::string>());
    file->timing_mode = StringTo<TimingMode>(j["Timing Mode"].get<std::string>());
    file->total_frames = j["Frames in File"];
    file->subfile_rows = j["Pixels"]["y"];
    file->subfile_cols = j["Pixels"]["x"];
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

    file->geometry = {j["Geometry"]["y"], j["Geometry"]["x"]};
}

void RawFileFactory::open_subfiles(File *_file) {
    auto file = dynamic_cast<RawFile *>(_file);
    for (size_t i = 0; i != file->n_subfiles; ++i) {
        auto v = std::vector<SubFile *>(file->n_subfile_parts);
        for (size_t j = 0; j != file->n_subfile_parts; ++j) {
            v[j] =
                new SubFile(file->data_fname(i, j), file->type, file->subfile_rows, file->subfile_cols, file->bitdepth);
        }
        file->subfiles.push_back(v);
    }
}

RawFile *RawFileFactory::load_file() {
    RawFile *file = new RawFile();
    file->fname = this->m_fpath;
    this->parse_fname(file);
    this->parse_metadata(file);
    file->find_number_of_subfiles();

    this->find_geometry(file);
    this->open_subfiles(file);

    return file;
}

sls_detector_header RawFileFactory::read_header(const std::filesystem::path &fname) {
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

void RawFileFactory::find_geometry(File *_file) {
    auto file = dynamic_cast<RawFile *>(_file);
    uint16_t r{};
    uint16_t c{};
    for (size_t i = 0; i < file->n_subfile_parts; i++) {
        for (size_t j = 0; j != file->n_subfiles; ++j) {
            auto h = this->read_header(file->data_fname(j, i));
            r = std::max(r, h.row);
            c = std::max(c, h.column);

            file->positions.push_back({h.row, h.column});
        }
    }

    r++;
    c++;

    file->rows = r * file->subfile_rows;
    file->cols = c * file->subfile_cols;

    file->rows += (r - 1) * file->cfg.module_gap_row;
}

void RawFileFactory::parse_fname(File *file) {

    file->base_path = this->m_fpath.parent_path();
    file->base_name = this->m_fpath.stem();
    file->ext = this->m_fpath.extension();

    auto pos = file->base_name.rfind("_");
    file->findex = std::stoi(file->base_name.substr(pos + 1));
    pos = file->base_name.find("_master_");
    file->base_name.erase(pos);
}
