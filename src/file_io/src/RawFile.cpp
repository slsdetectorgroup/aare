#include "aare/file_io/RawFile.hpp"
#include "aare/core/defs.hpp"
#include "aare/utils/json.hpp"
#include "aare/utils/logger.hpp"
#include <fmt/format.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace aare {

RawFile::RawFile(const std::filesystem::path &fname, const std::string &mode, const FileConfig &config) {
    m_mode = mode;
    m_fname = fname;
    if (mode == "r" || mode == "r+") {
        if (config != FileConfig()) {
            aare::logger::warn(
                "In read mode it is not necessary to provide a config, the provided config will be ignored");
        }
        parse_fname();
        parse_metadata();
        find_number_of_subfiles();
        find_geometry();
        open_subfiles();

    } else if (mode == "w" || mode == "w+") {

        if (std::filesystem::exists(fname)) {
            // handle mode w as w+ (no overrwriting)
            throw std::runtime_error(LOCATION + "File already exists");
        }

        parse_config(config);
        parse_fname();
        write_master_file();
        n_subfiles = 1;
        n_subfile_parts = 1;
        subfile_cols = m_cols;
        subfile_rows = m_rows;
        open_subfiles();

    } else {
        throw std::runtime_error(LOCATION + "Unsupported mode");
    }
}

void RawFile::parse_config(const FileConfig &config) {
    m_bitdepth = config.dtype.bitdepth();
    m_total_frames = config.total_frames;
    m_rows = config.rows;
    m_cols = config.cols;
    m_type = config.detector_type;
    max_frames_per_file = config.max_frames_per_file;
    m_geometry = config.geometry;
    version = config.version;
    subfile_rows = config.geometry.row;
    subfile_cols = config.geometry.col;

    if (m_geometry != aare::xy{1, 1}) {
        throw std::runtime_error(LOCATION + "Only geometry {1,1} files are supported for writing");
    }
}
void RawFile::write_master_file() {
    if (m_ext != ".json") {
        throw std::runtime_error(LOCATION + "only json master files are supported for writing");
    }
    std::ofstream ofs(master_fname(), std::ios::binary);
    std::string ss;
    ss.reserve(1024);
    ss += "{\n\t";
    aare::write_str(ss, "Version", version);
    ss += "\n\t";
    aare::write_digit(ss, "Total Frames", m_total_frames);
    ss += "\n\t";
    aare::write_str(ss, "Detector Type", toString(m_type));
    ss += "\n\t";
    aare::write_str(ss, "Geometry", m_geometry.to_string());
    ss += "\n\t";

    uint64_t img_size = (m_cols * m_rows) / (static_cast<size_t>(m_geometry.col * m_geometry.row));
    img_size *= m_bitdepth;
    aare::write_digit(ss, "Image Size in bytes", img_size);
    ss += "\n\t";
    aare::write_digit(ss, "Max Frames Per File", max_frames_per_file);
    ss += "\n\t";
    aare::write_digit(ss, "Dynamic Range", m_bitdepth);
    ss += "\n\t";
    const aare::xy pixels = {static_cast<uint32_t>(m_rows / m_geometry.row),
                             static_cast<uint32_t>(m_cols / m_geometry.col)};
    aare::write_str(ss, "Pixels", pixels.to_string());
    ss += "\n\t";
    aare::write_digit(ss, "Number of rows", m_rows);
    ss += "\n\t";
    const std::string tmp = "{\n"
                            "        \"Frame Number\": \"8 bytes\",\n"
                            "        \"Exposure Length\": \"4 bytes\",\n"
                            "        \"Packet Number\": \"4 bytes\",\n"
                            "        \"Bunch Id\": \"8 bytes\",\n"
                            "        \"Timestamp\": \"8 bytes\",\n"
                            "        \"Module Id\": \"2 bytes\",\n"
                            "        \"Row\": \"2 bytes\",\n"
                            "        \"Column\": \"2 bytes\",\n"
                            "        \"Reserved\": \"2 bytes\",\n"
                            "        \"Debug\": \"4 bytes\",\n"
                            "        \"RoundRNumber\": \"2 bytes\",\n"
                            "        \"DetType\": \"1 byte\",\n"
                            "        \"Version\": \"1 byte\",\n"
                            "        \"Packet Mask\": \"64 bytes\"\n"
                            "    }";

    ss += "\"Frame Header Format\":" + tmp + "\n";
    ss += "}";
    ofs << ss;
    ofs.close();
}

void RawFile::open_subfiles() {
    if (m_mode == "r")
        for (size_t i = 0; i != n_subfiles; ++i) {
            auto v = std::vector<SubFile *>(n_subfile_parts);
            for (size_t j = 0; j != n_subfile_parts; ++j) {
                v[j] = new SubFile(data_fname(i, j), m_type, subfile_rows, subfile_cols, m_bitdepth);
            }
            subfiles.push_back(v);
        }
    else {
        auto v = std::vector<SubFile *>(n_subfile_parts); // only one subfile is implemented
        v[0] = new SubFile(data_fname(0, 0), m_type, m_rows, m_cols, m_bitdepth, "w");
        subfiles.push_back(v);
    }
}

sls_detector_header RawFile::read_header(const std::filesystem::path &fname) {
    sls_detector_header h{};
    FILE *fp = fopen(fname.string().c_str(), "r");
    if (!fp)
        throw std::runtime_error(fmt::format("Could not open: {} for reading", fname.string()));

    size_t const rc = fread(reinterpret_cast<char *>(&h), sizeof(h), 1, fp);
    if (rc != 1)
        throw std::runtime_error(LOCATION + "Could not read header from file");
    if (fclose(fp)) {
        throw std::runtime_error(LOCATION + "Could not close file");
    }

    return h;
}
bool RawFile::is_master_file(const std::filesystem::path &fpath) {
    std::string const stem = fpath.stem().string();
    return stem.find("_master_") != std::string::npos;
}

void RawFile::find_number_of_subfiles() {
    int n_mod = 0;
    while (std::filesystem::exists(data_fname(++n_mod, 0)))
        ;
    n_subfiles = n_mod;
}
inline std::filesystem::path RawFile::data_fname(size_t mod_id, size_t file_id) {
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

    m_rows = (r * subfile_rows);
    m_cols = (c * subfile_cols);

    m_rows += static_cast<size_t>((r - 1) * cfg.module_gap_row);
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
    n_subfile_parts = static_cast<size_t>(m_geometry.row) * m_geometry.col;
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

    m_geometry = {j["Geometry"]["y"], j["Geometry"]["x"]};
}
void RawFile::parse_raw_metadata() {
    std::ifstream ifs(master_fname());
    for (std::string line; std::getline(ifs, line);) {
        if (line == "#Frame Header")
            break;
        auto pos = line.find(':');
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
                m_geometry = {static_cast<uint32_t>(std::stoi(value.substr(1, pos))),
                            static_cast<uint32_t>(std::stoi(value.substr(pos + 1)))};
            }
        }
    }
}

void RawFile::parse_fname() {
    bool wrong_format = false;
    m_base_path = m_fname.parent_path().string();
    m_base_name = m_fname.stem().string();
    m_ext = m_fname.extension().string();
    try {
        auto pos = m_base_name.rfind('_');
        m_findex = std::stoi(m_base_name.substr(pos + 1));
    } catch (const std::invalid_argument &e) {
        m_findex = 0;
        wrong_format = true;
    }
    auto pos = m_base_name.find("_master_");
    if (pos != std::string::npos) {
        m_base_name.erase(pos);
        wrong_format = true;
    }
    if (wrong_format && (m_mode == "w+" || m_mode == "w")) {
        aare::logger::warn("Master Filename", m_fname, "is not in the correct format");
        aare::logger::warn("using", master_fname(), "as the master file");
    }
}

Frame RawFile::get_frame(size_t frame_index) {
    auto f = Frame(this->m_rows, this->m_cols, this->m_bitdepth);
    std::byte *frame_buffer = f.data();
    get_frame_into(frame_index, frame_buffer);
    return f;
}

void RawFile::get_frame_into(size_t frame_index, std::byte *frame_buffer) {
    if (frame_index > this->m_total_frames) {
        throw std::runtime_error(LOCATION + "Frame number out of range");
    }
    std::vector<size_t> frame_numbers(this->n_subfile_parts);
    std::vector<size_t> frame_indices(this->n_subfile_parts, frame_index);

    if (n_subfile_parts != 1) {
        for (size_t part_idx = 0; part_idx != this->n_subfile_parts; ++part_idx) {
            auto subfile_id = frame_index / this->max_frames_per_file;
            frame_numbers[part_idx] =
                this->subfiles[subfile_id][part_idx]->frame_number(frame_index % this->max_frames_per_file);
        }
        // 1. if frame number vector is the same break
        while (std::adjacent_find(frame_numbers.begin(), frame_numbers.end(), std::not_equal_to<>()) !=
               frame_numbers.end()) {
            // 2. find the index of the minimum frame number,
            auto min_frame_idx =
                std::distance(frame_numbers.begin(), std::min_element(frame_numbers.begin(), frame_numbers.end()));
            // 3. increase its index and update its respective frame number
            frame_indices[min_frame_idx]++;
            // 4. if we can't increase its index => throw error
            if (frame_indices[min_frame_idx] >= this->m_total_frames) {
                throw std::runtime_error(LOCATION + "Frame number out of range");
            }
            auto subfile_id = frame_indices[min_frame_idx] / this->max_frames_per_file;
            frame_numbers[min_frame_idx] = this->subfiles[subfile_id][min_frame_idx]->frame_number(
                frame_indices[min_frame_idx] % this->max_frames_per_file);
        }
    }

    if (this->m_geometry.col == 1) {
        // get the part from each subfile and copy it to the frame
        for (size_t part_idx = 0; part_idx != this->n_subfile_parts; ++part_idx) {
            auto corrected_idx = frame_indices[part_idx];
            auto subfile_id = corrected_idx / this->max_frames_per_file;
            auto part_offset = this->subfiles[subfile_id][part_idx]->bytes_per_part();
            this->subfiles[subfile_id][part_idx]->get_part(frame_buffer + part_idx * part_offset,
                                                           corrected_idx % this->max_frames_per_file);
        }

    } else {

        // create a buffer that will hold a the frame part
        auto bytes_per_part = this->subfile_rows * this->subfile_cols * this->m_bitdepth / 8;
        auto *part_buffer = new std::byte[bytes_per_part];

        for (size_t part_idx = 0; part_idx != this->n_subfile_parts; ++part_idx) {
            auto corrected_idx = frame_indices[part_idx];
            auto subfile_id = corrected_idx / this->max_frames_per_file;

            this->subfiles[subfile_id][part_idx]->get_part(part_buffer, corrected_idx % this->max_frames_per_file);
            for (size_t cur_row = 0; cur_row < (this->subfile_rows); cur_row++) {
                auto irow = cur_row + (part_idx / this->m_geometry.col) * this->subfile_rows;
                auto icol = (part_idx % this->m_geometry.col) * this->subfile_cols;
                auto dest = (irow * this->m_cols + icol);
                dest = dest * this->m_bitdepth / 8;
                memcpy(frame_buffer + dest, part_buffer + cur_row * this->subfile_cols * this->m_bitdepth / 8,
                       this->subfile_cols * this->m_bitdepth / 8);
            }
        }
        delete[] part_buffer;
    }
}

void RawFile::write(Frame &frame, sls_detector_header header) {
    if (m_mode == "r") {
        throw std::runtime_error(LOCATION + "File is open in read mode");
    }
    size_t const subfile_id = this->current_frame / this->max_frames_per_file;
    for (size_t part_idx = 0; part_idx != this->n_subfile_parts; ++part_idx) {

        this->subfiles[subfile_id][part_idx]->write_part(frame.data(), header,
                                                         this->current_frame % this->max_frames_per_file);
    }
    this->current_frame++;
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
    size_t const subfile_id = frame_index / this->max_frames_per_file;
    return this->subfiles[subfile_id][0]->frame_number(frame_index % this->max_frames_per_file);
}

RawFile::~RawFile() noexcept {

    // update master file
    if (m_mode == "w" || m_mode == "w+" || m_mode == "r+") {
        try {
            write_master_file();
        } catch (...) {
            aare::logger::warn(LOCATION + "Could not update master file");
        }
    }

    for (auto &vec : subfiles) {
        for (auto *subfile : vec) {
            delete subfile;
        }
    }
}

} // namespace aare