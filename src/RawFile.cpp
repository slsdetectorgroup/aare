#include "aare/RawFile.hpp"
#include "aare/PixelMap.hpp"
#include "aare/defs.hpp"
#include "aare/geo_helpers.hpp"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace aare {

RawFile::RawFile(const std::filesystem::path &fname, const std::string &mode)
    : m_master(fname) {
    m_mode = mode;
    if (mode == "r") {

        n_subfiles = find_number_of_subfiles(); // f0,f1...fn
        n_subfile_parts =
            m_master.geometry().col * m_master.geometry().row; // d0,d1...dn

        

        find_geometry();

        if (m_master.roi()){
            m_geometry = update_geometry_with_roi(m_geometry, m_master.roi().value());
        }
        
        open_subfiles();
    } else {
        throw std::runtime_error(LOCATION +
                                 "Unsupported mode. Can only read RawFiles.");
    }
}

Frame RawFile::read_frame() { return get_frame(m_current_frame++); };

Frame RawFile::read_frame(size_t frame_number) {
    seek(frame_number);
    return read_frame();
}

void RawFile::read_into(std::byte *image_buf, size_t n_frames) {
    // TODO: implement this in a more efficient way

    for (size_t i = 0; i < n_frames; i++) {
        this->get_frame_into(m_current_frame++, image_buf);
        image_buf += bytes_per_frame();
    }
}

void RawFile::read_into(std::byte *image_buf) {
    return get_frame_into(m_current_frame++, image_buf);
};


void RawFile::read_into(std::byte *image_buf, DetectorHeader *header) {

    return get_frame_into(m_current_frame++, image_buf, header);
};

void RawFile::read_into(std::byte *image_buf, size_t n_frames, DetectorHeader *header) {
    // return get_frame_into(m_current_frame++, image_buf, header);

    for (size_t i = 0; i < n_frames; i++) {
        this->get_frame_into(m_current_frame++, image_buf, header);
        image_buf += bytes_per_frame();
        if(header) 
            header+=n_mod();
    }

};

size_t RawFile::n_mod() const { return n_subfile_parts; }


size_t RawFile::bytes_per_frame() {
    return m_geometry.pixels_x * m_geometry.pixels_y * m_master.bitdepth() / bits_per_byte;
}
size_t RawFile::pixels_per_frame() { 
    // return m_rows * m_cols; 
    return m_geometry.pixels_x * m_geometry.pixels_y;
}

DetectorType RawFile::detector_type() const { return m_master.detector_type(); }

void RawFile::seek(size_t frame_index) {
    // check if the frame number is greater than the total frames
    // if frame_number == total_frames, then the next read will throw an error
    if (frame_index > total_frames()) {
        throw std::runtime_error(
            fmt::format("frame number {} is greater than total frames {}",
                        frame_index, total_frames()));
    }
    m_current_frame = frame_index;
};

size_t RawFile::tell() { return m_current_frame; };

size_t RawFile::total_frames() const { return m_master.frames_in_file(); }
size_t RawFile::rows() const { return m_geometry.pixels_y; }
size_t RawFile::cols() const { return m_geometry.pixels_x; }
size_t RawFile::bitdepth() const { return m_master.bitdepth(); }
xy RawFile::geometry() { return m_master.geometry(); }

void RawFile::open_subfiles() {
    if (m_mode == "r")
        for (size_t i = 0; i != n_subfiles; ++i) {
            auto v = std::vector<RawSubFile *>(n_subfile_parts);
            for (size_t j = 0; j != n_subfile_parts; ++j) {
                auto pos = m_geometry.module_pixel_0[j];
                v[j] = new RawSubFile(m_master.data_fname(j, i),
                                      m_master.detector_type(), pos.height,
                                      pos.width, m_master.bitdepth(),
                                      pos.row_index, pos.col_index);

            }
            subfiles.push_back(v);
        }
    else {
        throw std::runtime_error(LOCATION +
                                 "Unsupported mode. Can only read RawFiles.");
    }
}

DetectorHeader RawFile::read_header(const std::filesystem::path &fname) {
    DetectorHeader h{};
    FILE *fp = fopen(fname.string().c_str(), "r");
    if (!fp)
        throw std::runtime_error(
            fmt::format("Could not open: {} for reading", fname.string()));

    size_t const rc = fread(reinterpret_cast<char *>(&h), sizeof(h), 1, fp);
    if (rc != 1)
        throw std::runtime_error(LOCATION + "Could not read header from file");
    if (fclose(fp)) {
        throw std::runtime_error(LOCATION + "Could not close file");
    }

    return h;
}

int RawFile::find_number_of_subfiles() {
    int n_files = 0;
    // f0,f1...fn How many files is the data split into?
    while (std::filesystem::exists(m_master.data_fname(0, n_files)))
        n_files++; // increment after test

#ifdef AARE_VERBOSE
    fmt::print("Found: {} subfiles\n", n_files);
#endif
    return n_files;

}

RawMasterFile RawFile::master() const { return m_master; }

/**
 * @brief Find the geometry of the detector by opening all the subfiles and
 * reading the headers. 
 */
void RawFile::find_geometry() {
    
    //Hold the maximal row and column number found
    //Later used for calculating the total number of rows and columns
    uint16_t r{};
    uint16_t c{};


    for (size_t i = 0; i < n_subfile_parts; i++) {
        auto h = read_header(m_master.data_fname(i, 0));
        r = std::max(r, h.row);
        c = std::max(c, h.column);
        // positions.push_back({h.row, h.column});
       
        ModuleGeometry g;
        g.origin_x = h.column * m_master.pixels_x();
        g.origin_y = h.row * m_master.pixels_y();
        g.row_index = h.row;
        g.col_index = h.column;
        g.width = m_master.pixels_x();
        g.height = m_master.pixels_y();
        m_geometry.module_pixel_0.push_back(g);

    }

    r++;
    c++;

    m_geometry.pixels_y = (r * m_master.pixels_y());
    m_geometry.pixels_x  = (c * m_master.pixels_x());
    m_geometry.modules_x = c;
    m_geometry.modules_y = r;
    m_geometry.pixels_y += static_cast<size_t>((r - 1) * cfg.module_gap_row);

}


Frame RawFile::get_frame(size_t frame_index) {
    auto f = Frame(m_geometry.pixels_y, m_geometry.pixels_x, Dtype::from_bitdepth(m_master.bitdepth()));
    std::byte *frame_buffer = f.data();
    get_frame_into(frame_index, frame_buffer);
    return f;
}


size_t RawFile::bytes_per_pixel() const {
    return m_master.bitdepth() / 8;
}

void RawFile::get_frame_into(size_t frame_index, std::byte *frame_buffer, DetectorHeader *header) {
    if (frame_index >= total_frames()) {
        throw std::runtime_error(LOCATION + "Frame number out of range");
    }
    std::vector<size_t> frame_numbers(n_subfile_parts);
    std::vector<size_t> frame_indices(n_subfile_parts, frame_index);


    // sync the frame numbers

    if (n_subfile_parts != 1) {
        for (size_t part_idx = 0; part_idx != n_subfile_parts; ++part_idx) {
            auto subfile_id = frame_index / m_master.max_frames_per_file();
            if (subfile_id >= subfiles.size()) {
                throw std::runtime_error(LOCATION +
                                         " Subfile out of range. Possible missing data.");
            }
            frame_numbers[part_idx] =
                subfiles[subfile_id][part_idx]->frame_number(
                    frame_index % m_master.max_frames_per_file());
        }
        // 1. if frame number vector is the same break
        while (std::adjacent_find(frame_numbers.begin(), frame_numbers.end(),
                                  std::not_equal_to<>()) !=
               frame_numbers.end()) {
            // 2. find the index of the minimum frame number,
            auto min_frame_idx = std::distance(
                frame_numbers.begin(),
                std::min_element(frame_numbers.begin(), frame_numbers.end()));
            // 3. increase its index and update its respective frame number
            frame_indices[min_frame_idx]++;
            // 4. if we can't increase its index => throw error
            if (frame_indices[min_frame_idx] >= total_frames()) {
                throw std::runtime_error(LOCATION +
                                         "Frame number out of range");
            }
            auto subfile_id =
                frame_indices[min_frame_idx] / m_master.max_frames_per_file();
            frame_numbers[min_frame_idx] =
                subfiles[subfile_id][min_frame_idx]->frame_number(
                    frame_indices[min_frame_idx] %
                    m_master.max_frames_per_file());
        }
    }

    if (m_master.geometry().col == 1) {
        // get the part from each subfile and copy it to the frame
        for (size_t part_idx = 0; part_idx != n_subfile_parts; ++part_idx) {
            auto corrected_idx = frame_indices[part_idx];
            auto subfile_id = corrected_idx / m_master.max_frames_per_file();
            if (subfile_id >= subfiles.size()) {
                throw std::runtime_error(LOCATION +
                                         " Subfile out of range. Possible missing data.");
            }

            // This is where we start writing
            auto offset = (m_geometry.module_pixel_0[part_idx].origin_y * m_geometry.pixels_x +
                m_geometry.module_pixel_0[part_idx].origin_x)*m_master.bitdepth()/8;

            if (m_geometry.module_pixel_0[part_idx].origin_x!=0)
                throw std::runtime_error(LOCATION + "Implementation error. x pos not 0.");
            
            //TODO! Risk for out of range access
            subfiles[subfile_id][part_idx]->seek(corrected_idx % m_master.max_frames_per_file());
            subfiles[subfile_id][part_idx]->read_into(frame_buffer + offset, header);
            if (header)
                ++header;
        }

    } else {
        //TODO! should we read row by row?

        // create a buffer large enough to hold a full module

        auto bytes_per_part = m_master.pixels_y() * m_master.pixels_x() *
                              m_master.bitdepth() /
                              8; // TODO! replace with image_size_in_bytes
        auto *part_buffer = new std::byte[bytes_per_part];

        // TODO! if we have many submodules we should reorder them on the module
        // level

        for (size_t part_idx = 0; part_idx != n_subfile_parts; ++part_idx) {
            auto pos = m_geometry.module_pixel_0[part_idx];
            auto corrected_idx = frame_indices[part_idx];
            auto subfile_id = corrected_idx / m_master.max_frames_per_file();
            if (subfile_id >= subfiles.size()) {
                throw std::runtime_error(LOCATION +
                                         " Subfile out of range. Possible missing data.");
            }

            subfiles[subfile_id][part_idx]->seek(corrected_idx % m_master.max_frames_per_file());
            subfiles[subfile_id][part_idx]->read_into(part_buffer, header);
            if(header)
                ++header;

            for (size_t cur_row = 0; cur_row < static_cast<size_t>(pos.height);
                 cur_row++) {

                auto irow = (pos.origin_y + cur_row);
                auto icol = pos.origin_x;
                auto dest = (irow * this->m_geometry.pixels_x + icol);
                dest = dest * m_master.bitdepth() / 8;
                memcpy(frame_buffer + dest,
                       part_buffer + cur_row * pos.width *
                                         m_master.bitdepth() / 8,
                       pos.width * m_master.bitdepth() / 8);

            }
        }
        delete[] part_buffer;
    }
}

std::vector<Frame> RawFile::read_n(size_t n_frames) {
    // TODO: implement this in a more efficient way
    std::vector<Frame> frames;
    for (size_t i = 0; i < n_frames; i++) {
        frames.push_back(this->get_frame(m_current_frame));
        m_current_frame++;
    }
    return frames;
}

size_t RawFile::frame_number(size_t frame_index) {
    if (frame_index >= m_master.frames_in_file()) {
        throw std::runtime_error(LOCATION + " Frame number out of range");
    }
    size_t subfile_id = frame_index / m_master.max_frames_per_file();
    if (subfile_id >= subfiles.size()) {
        throw std::runtime_error(
            LOCATION + " Subfile out of range. Possible missing data.");
    }
    return subfiles[subfile_id][0]->frame_number(
        frame_index % m_master.max_frames_per_file());
}

RawFile::~RawFile() {

    // TODO! Fix this, for file closing
    for (auto &vec : subfiles) {
        for (auto *subfile : vec) {
            delete subfile;
        }
    }
}





} // namespace aare