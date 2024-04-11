#include "aare/file_io/RawFile.hpp"
#include "aare/core/defs.hpp"
#include "aare/utils/logger.hpp"
#include <fmt/format.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace aare {

RawFile::RawFile(const std::filesystem::path &fname, const std::string &mode, const FileConfig &config) {
    m_fname = fname;
    if (mode == "r") {
        if (config != FileConfig()) {
            aare::logger::warn(
                "In read mode it is not necessary to provide a config, the provided config will be ignored");
        }
        parse_fname();
        parse_metadata();
        find_number_of_subfiles();
        find_geometry();
        open_subfiles();

    } else {
        throw std::runtime_error(LOCATION + "Unsupported mode");
    }
}

void RawFile::open_subfiles() {
    for (size_t i = 0; i != n_subfiles; ++i) {
        auto v = std::vector<SubFile *>(n_subfile_parts);
        for (size_t j = 0; j != n_subfile_parts; ++j) {
            v[j] = new SubFile(data_fname(i, j), m_type, subfile_rows, subfile_cols, bitdepth());
        }
        subfiles.push_back(v);
    }
}

sls_detector_header RawFile::read_header(const std::filesystem::path &fname) {
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
bool RawFile::is_master_file(std::filesystem::path fpath) {
    std::string stem = fpath.stem();
    if (stem.find("_master_") != std::string::npos)
        return true;
    else
        return false;
}

void RawFile::find_number_of_subfiles() {
    int n_mod = 0;
    while (std::filesystem::exists(data_fname(++n_mod, 0)))
        ;
    n_subfiles = n_mod;
}
inline std::filesystem::path RawFile::data_fname(int mod_id, int file_id) {
    return this->m_base_path / fmt::format("{}_d{}_f{}_{}.raw", this->m_base_name, file_id, mod_id, this->m_findex);
}

inline std::filesystem::path RawFile::master_fname() {
    return this->m_base_path / fmt::format("{}_master_{}{}", this->m_base_name, this->m_findex, this->m_ext);
}

void RawFile::find_geometry() {
    uint16_t r{};
    uint16_t c{};
    for (size_t i = 0; i < n_subfile_parts; i++) {
        for (size_t j = 0; j != n_subfiles; ++j) {
            auto h = this->read_header(data_fname(j, i));
            r = std::max(r, h.row);
            c = std::max(c, h.column);

            positions.push_back({h.row, h.column});
        }
    }

    r++;
    c++;

    m_rows = r * subfile_rows;
    m_cols = c * subfile_cols;

    m_rows += (r - 1) * cfg.module_gap_row;
}

void RawFile::parse_metadata() {
    if (m_ext == ".raw") {
        parse_raw_metadata();
        if (m_bitdepth == 0) {
            switch (m_type) {
            case DetectorType::Eiger:
                m_bitdepth = 32;
                break;
            default:
                m_bitdepth = 16;
            }
        }
    } else if (m_ext == ".json") {
        parse_json_metadata();
    } else {
        throw std::runtime_error(LOCATION + "Unsupported file type");
    }
    n_subfile_parts = geometry.row * geometry.col;
}

void RawFile::parse_json_metadata() {
    std::ifstream ifs(master_fname());
    json j;
    ifs >> j;
    double v = j["Version"];
    version = fmt::format("{:.1f}", v);
    m_type = StringTo<DetectorType>(j["Detector Type"].get<std::string>());
    timing_mode = StringTo<TimingMode>(j["Timing Mode"].get<std::string>());
    m_total_frames = j["Frames in File"];
    subfile_rows = j["Pixels"]["y"];
    subfile_cols = j["Pixels"]["x"];
    max_frames_per_file = j["Max Frames Per File"];
    try {
        m_bitdepth = j.at("Dynamic Range");
    } catch (const json::out_of_range &e) {
        m_bitdepth = 16;
    }
    // only Eiger had quad
    if (m_type == DetectorType::Eiger) {
        quad = (j["Quad"] == 1);
    }

    geometry = {j["Geometry"]["y"], j["Geometry"]["x"]};
}
void RawFile::parse_raw_metadata() {
    std::ifstream ifs(master_fname());
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
                version = value;
            } else if (key == "TimeStamp") {

            } else if (key == "Detector Type") {
                m_type = StringTo<DetectorType>(value);
            } else if (key == "Timing Mode") {
                timing_mode = StringTo<TimingMode>(value);
            } else if (key == "Pixels") {
                // Total number of pixels cannot be found yet looking at
                // submodule
                pos = value.find(',');
                subfile_cols = std::stoi(value.substr(1, pos));
                subfile_rows = std::stoi(value.substr(pos + 1));
            } else if (key == "Total Frames") {
                m_total_frames = std::stoi(value);
            } else if (key == "Dynamic Range") {
                m_bitdepth = std::stoi(value);
            } else if (key == "Quad") {
                quad = (value == "1");
            } else if (key == "Max Frames Per File") {
                max_frames_per_file = std::stoi(value);
            } else if (key == "Geometry") {
                pos = value.find(',');
                geometry = {std::stoi(value.substr(1, pos)), std::stoi(value.substr(pos + 1))};
            }
        }
    }
}

void RawFile::parse_fname() {
    m_base_path = m_fname.parent_path();
    m_base_name = m_fname.stem();
    m_ext = m_fname.extension();
    auto pos = m_base_name.rfind("_");
    m_findex = std::stoi(m_base_name.substr(pos + 1));
    pos = m_base_name.find("_master_");
    m_base_name.erase(pos);
}

Frame RawFile::get_frame(size_t frame_number) {
    auto f = Frame(this->m_rows, this->m_cols, this->m_bitdepth);
    std::byte *frame_buffer = f.data();
    get_frame_into(frame_number, frame_buffer);
    return f;
}

void RawFile::get_frame_into(size_t frame_number, std::byte *frame_buffer) {
    if (frame_number > this->m_total_frames) {
        throw std::runtime_error(LOCATION + "Frame number out of range");
    }
    int subfile_id = frame_number / this->max_frames_per_file;
    // create frame and get its buffer

    if (this->geometry.col == 1) {
        // get the part from each subfile and copy it to the frame
        for (size_t part_idx = 0; part_idx != this->n_subfile_parts; ++part_idx) {
            auto part_offset = this->subfiles[subfile_id][part_idx]->bytes_per_part();
            this->subfiles[subfile_id][part_idx]->get_part(frame_buffer + part_idx * part_offset,
                                                           frame_number % this->max_frames_per_file);
        }

    } else {
        // create a buffer that will hold a the frame part
        auto bytes_per_part = this->subfile_rows * this->subfile_cols * this->m_bitdepth / 8;
        std::byte *part_buffer = new std::byte[bytes_per_part];

        for (size_t part_idx = 0; part_idx != this->n_subfile_parts; ++part_idx) {
            this->subfiles[subfile_id][part_idx]->get_part(part_buffer, frame_number % this->max_frames_per_file);
            for (int cur_row = 0; cur_row < (this->subfile_rows); cur_row++) {
                auto irow = cur_row + (part_idx / this->geometry.col) * this->subfile_rows;
                auto icol = (part_idx % this->geometry.col) * this->subfile_cols;
                auto dest = (irow * this->m_cols + icol);
                dest = dest * this->m_bitdepth / 8;
                memcpy(frame_buffer + dest, part_buffer + cur_row * this->subfile_cols * this->m_bitdepth / 8,
                       this->subfile_cols * this->m_bitdepth / 8);
            }
        }
        delete[] part_buffer;
    }
}

std::vector<Frame> RawFile::read(size_t n_frames) {
    // TODO: implement this in a more efficient way
    std::vector<Frame> frames;
    for (size_t i = 0; i < n_frames; i++) {
        frames.push_back(this->get_frame(this->current_frame));
        this->current_frame++;
    }
    return frames;
}
void RawFile::read_into(std::byte *image_buf, size_t n_frames) {
    // TODO: implement this in a more efficient way
    for (size_t i = 0; i < n_frames; i++) {
        this->get_frame_into(this->current_frame++, image_buf);
        image_buf += this->bytes_per_frame();
    }
}

size_t RawFile::frame_number(size_t frame_index) {
    if (frame_index > this->m_total_frames) {
        throw std::runtime_error(LOCATION + "Frame number out of range");
    }
    int subfile_id = frame_index / this->max_frames_per_file;
    return this->subfiles[subfile_id][0]->frame_number(frame_index % this->max_frames_per_file);
}

RawFile::~RawFile() {
    for (auto &vec : subfiles) {
        for (auto subfile : vec) {
            delete subfile;
        }
    }
}

} // namespace aare