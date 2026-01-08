// SPDX-License-Identifier: MPL-2.0
#include "aare/RawFile.hpp"
#include "aare/DetectorGeometry.hpp"
#include "aare/PixelMap.hpp"
#include "aare/ROIGeometry.hpp"
#include "aare/algorithm.hpp"
#include "aare/defs.hpp"
#include "aare/logger.hpp"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace aare {

RawFile::RawFile(const std::filesystem::path &fname, const std::string &mode)
    : m_master(fname),
      m_geometry(m_master.geometry(), m_master.pixels_x(), m_master.pixels_y(),
                 m_master.udp_interfaces_per_module(), m_master.quad()) {

    m_mode = mode;

    m_subfiles.resize(m_master.rois().has_value() ? m_master.rois()->size()
                                                  : 1);

    if (mode == "r") {
        if (m_master.rois().has_value()) {
            m_ROI_geometries.reserve(m_master.rois()->size());

            // iterate over all ROIS
            size_t roi_index = 0;
            const auto rois = m_master.rois().value();
            for (const auto &roi : rois) {
                m_ROI_geometries.push_back(ROIGeometry(roi, m_geometry));
                // open subfiles
                open_subfiles(roi_index);
                ++roi_index;
            }

        } else {
            // no ROI use full detector
            m_ROI_geometries.reserve(1);
            m_ROI_geometries.push_back(ROIGeometry(m_geometry));
            open_subfiles(0);
        }
    } else {
        throw std::runtime_error(LOCATION +
                                 " Unsupported mode. Can only read RawFiles.");
    }
}

std::vector<Frame> RawFile::read_ROIs(const std::optional<size_t> roi_index) {

    if (!m_master.rois()) {
        throw std::runtime_error(LOCATION +
                                 "No ROIs defined in the master file.");
    }

    const size_t num_rois = roi_index.has_value() ? 1 : m_ROI_geometries.size();

    std::vector<Frame> frames;
    frames.reserve(num_rois);

    if (roi_index.has_value()) {
        if (roi_index.value() >= num_rois) {
            throw std::runtime_error(LOCATION + "ROI index out of range.");
        }
        frames.push_back(get_frame(
            m_current_frame++,
            roi_index
                .value())); // TODO: potentially buggy - frame index incremented
                            // user cannot read with other ROI afterwards
        return frames;
    }

    for (size_t roi_idx = 0; roi_idx < num_rois; ++roi_idx) {
        frames.push_back(get_frame(m_current_frame, roi_idx));
    }
    ++m_current_frame;

    return frames;
}

std::vector<Frame> RawFile::read_ROIs(const size_t frame_number,
                                      const std::optional<size_t> roi_index) {

    if (!m_master.rois()) {
        throw std::runtime_error(LOCATION +
                                 "No ROIs defined in the master file.");
    }

    std::vector<Frame> frames;
    frames.reserve(m_master.rois().value().size());

    seek(frame_number);

    if (roi_index.has_value()) {
        if (roi_index.value() >= m_master.rois()->size()) {
            throw std::runtime_error(LOCATION + "ROI index out of range.");
        }
        frames.push_back(get_frame(frame_number, roi_index.value()));
    } else {

        for (size_t roi_idx = 0; roi_idx < m_master.rois()->size(); ++roi_idx) {
            frames.push_back(get_frame(frame_number, roi_idx));
        }
    }

    ++m_current_frame;
    return frames;
}

Frame RawFile::read_frame() {
    if (m_master.rois().has_value() && m_master.rois()->size() > 1) {
        throw std::runtime_error(LOCATION +
                                 "Multiple ROIs defined in the master file. "
                                 "Use read_ROIs() instead.");
    }
    return get_frame(m_current_frame++);
}

Frame RawFile::read_frame(size_t frame_number) {
    if (m_master.rois().has_value() && m_master.rois()->size() > 1) {
        throw std::runtime_error(
            LOCATION + "Multiple ROIs defined in the master file. "
                       "Use read_ROIs(const size_t frame_number) instead.");
    }
    seek(frame_number);
    return read_frame();
}

void RawFile::read_into(std::byte *image_buf, size_t n_frames) {
    // TODO: implement this in a more efficient way
    if (m_master.rois().has_value() && m_master.rois()->size() > 1) {
        throw std::runtime_error(LOCATION +
                                 "Cannot use read_into for multiple ROIs.");
    }

    for (size_t i = 0; i < n_frames; i++) {
        this->get_frame_into(m_current_frame++, image_buf);
        image_buf += bytes_per_frame();
    }
}

void RawFile::read_into(std::byte *image_buf) {
    if (m_master.rois().has_value() && m_master.rois()->size() > 1) {
        throw std::runtime_error(LOCATION +
                                 "Cannot use read_into for multiple ROIs. Use "
                                 "read_roi_into() for a single ROI instead.");
    }
    return get_frame_into(m_current_frame++, image_buf);
}

void RawFile::read_roi_into(std::byte *image_buf, const size_t roi_index,
                            const size_t frame_number, DetectorHeader *header) {
    if (!m_master.rois().has_value()) {
        throw std::runtime_error(LOCATION +
                                 "No ROIs defined in the master file.");
    }
    if (roi_index >= num_rois()) {
        throw std::runtime_error(LOCATION + "ROI index out of range.");
    }
    return get_frame_into(frame_number, image_buf, roi_index, header);
}

void RawFile::read_into(std::byte *image_buf, DetectorHeader *header) {
    if (m_master.rois().has_value() && m_master.rois()->size() > 1) {
        throw std::runtime_error(LOCATION +
                                 "Cannot use read_into for multiple ROIs. Use "
                                 "read_roi_into() for a single ROI instead.");
    }
    return get_frame_into(m_current_frame++, image_buf, 0, header);
}

void RawFile::read_into(std::byte *image_buf, size_t n_frames,
                        DetectorHeader *header) {
    // return get_frame_into(m_current_frame++, image_buf, header);

    if (m_master.rois().has_value() && m_master.rois()->size() > 1) {
        throw std::runtime_error(
            LOCATION +
            "Cannot use read_into for multiple ROIs."); // TODO: maybe pass
                                                        // roi_index so one can
                                                        // use read_into for a
                                                        // specific ROI
    }

    for (size_t i = 0; i < n_frames; i++) {
        this->get_frame_into(m_current_frame++, image_buf, 0, header);
        image_buf += bytes_per_frame();
        if (header)
            header += m_ROI_geometries[0].num_modules_in_roi();
    }
}

size_t RawFile::bytes_per_frame() {
    if (m_master.rois().has_value() && m_master.rois()->size() > 1) {
        throw std::runtime_error(
            LOCATION + "Pass the desired roi_index to bytes_per_frame to get "
                       "bytes_per_frame for the specific ROI. ");
    }
    return bytes_per_frame(0);
}

size_t RawFile::bytes_per_frame(const size_t roi_index) {
    return m_ROI_geometries.at(roi_index).pixels_x() *
           m_ROI_geometries.at(roi_index).pixels_y() * m_master.bitdepth() /
           bits_per_byte;
}

size_t RawFile::pixels_per_frame() {
    if (m_master.rois().has_value() && m_master.rois()->size() > 1) {
        throw std::runtime_error(
            LOCATION + "Pass the desired roi_index to pixels_per_frame to get "
                       "pixels_per_frame for the specific ROI. ");
    }
    return pixels_per_frame(0);
}

size_t RawFile::pixels_per_frame(const size_t roi_index) {
    return m_ROI_geometries.at(roi_index).pixels_x() *
           m_ROI_geometries.at(roi_index).pixels_y();
}

DetectorType RawFile::detector_type() const { return m_master.detector_type(); }

void RawFile::seek(size_t frame_index) {
    // check if the frame number is greater than the total frames
    // if frame_number == total_frames, then the next read will throw an
    // error
    if (frame_index > total_frames()) {
        throw std::runtime_error(
            fmt::format("frame number {} is greater than total frames {}",
                        frame_index, total_frames()));
    }
    m_current_frame = frame_index;
}

size_t RawFile::tell() { return m_current_frame; }

size_t RawFile::total_frames() const { return m_master.frames_in_file(); }

size_t RawFile::rows() const {
    if (m_master.rois().has_value() && m_master.rois()->size() > 1) {
        throw std::runtime_error(LOCATION +
                                 "Pass the desired roi_index to rows to get "
                                 "rows for the specific ROI. ");
    }
    return rows(0);
}
size_t RawFile::rows(const size_t roi_index) const {
    return m_ROI_geometries.at(roi_index).pixels_y();
}
size_t RawFile::cols() const {
    if (m_master.rois().has_value() && m_master.rois()->size() > 1) {
        throw std::runtime_error(LOCATION +
                                 "Pass the desired roi_index to cols to get "
                                 "cols for the specific ROI. ");
    }
    return cols(0);
}
size_t RawFile::cols(const size_t roi_index) const {
    return m_ROI_geometries.at(roi_index).pixels_x();
}
size_t RawFile::bitdepth() const { return m_master.bitdepth(); }
xy RawFile::geometry() const {
    return xy{static_cast<uint32_t>(m_geometry.modules_y()),
              static_cast<uint32_t>(m_geometry.modules_x())};
}

size_t RawFile::n_modules() const { return m_geometry.n_modules(); };

size_t RawFile::num_rois() const {
    if (m_master.rois().has_value()) {
        return m_master.rois()->size();
    } else {
        return 0;
    }
};

const ROIGeometry &RawFile::roi_geometries(size_t roi_index) const {
    return m_ROI_geometries[roi_index];
}

std::vector<size_t> RawFile::n_modules_in_roi() const {

    std::vector<size_t> results(m_ROI_geometries.size());
    std::transform(
        m_ROI_geometries.begin(), m_ROI_geometries.end(), results.begin(),
        [](const ROIGeometry &roi) { return roi.num_modules_in_roi(); });
    return results;
};

void RawFile::open_subfiles(const size_t roi_index) {

    if (m_mode == "r") {

        m_subfiles[roi_index].reserve(
            m_ROI_geometries[roi_index].num_modules_in_roi());

        auto module_indices =
            m_ROI_geometries[roi_index].module_indices_in_roi();

        for (const size_t i :
             m_ROI_geometries[roi_index].module_indices_in_roi()) {
            const auto pos = m_geometry.get_module_geometries(i);
            m_subfiles[roi_index].emplace_back(std::make_unique<RawSubFile>(
                m_master.data_fname(i, 0), m_master.detector_type(), pos.height,
                pos.width, m_master.bitdepth(), pos.row_index, pos.col_index));
        }
    } else {
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

RawMasterFile RawFile::master() const { return m_master; }

Frame RawFile::get_frame(size_t frame_index, const size_t roi_index) {
    auto f = Frame(m_ROI_geometries[roi_index].pixels_y(),
                   m_ROI_geometries[roi_index].pixels_x(),
                   Dtype::from_bitdepth(m_master.bitdepth()));
    std::byte *frame_buffer = f.data();
    get_frame_into(frame_index, frame_buffer, roi_index);
    return f;
}

size_t RawFile::bytes_per_pixel() const { return m_master.bitdepth() / 8; }

void RawFile::get_frame_into(size_t frame_index, std::byte *frame_buffer,
                             const size_t roi_index, DetectorHeader *header) {
    LOG(logDEBUG) << "RawFile::get_frame_into(" << frame_index << ")";
    if (frame_index >= total_frames()) {
        throw std::runtime_error(LOCATION + "Frame number out of range");
    }
    std::vector<size_t> frame_numbers(
        m_ROI_geometries[roi_index].num_modules_in_roi());
    std::vector<size_t> frame_indices(
        m_ROI_geometries[roi_index].num_modules_in_roi(), frame_index);

    // sync the frame numbers

    if (m_ROI_geometries[roi_index].num_modules_in_roi() !=
        1) { // if we have more than one module
        for (size_t part_idx = 0;
             part_idx != m_ROI_geometries[roi_index].num_modules_in_roi();
             ++part_idx) {
            frame_numbers[part_idx] =
                m_subfiles[roi_index][part_idx]->frame_number(frame_index);
        }

        // 1. if frame number vector is the same break
        while (!all_equal(frame_numbers)) {

            // 2. find the index of the minimum frame number,
            auto min_frame_idx = std::distance(
                frame_numbers.begin(),
                std::min_element(frame_numbers.begin(), frame_numbers.end()));

            // 3. increase its index and update its respective frame
            // number
            frame_indices[min_frame_idx]++;

            // 4. if we can't increase its index => throw error
            if (frame_indices[min_frame_idx] >= total_frames()) {
                throw std::runtime_error(LOCATION +
                                         "Frame number out of range");
            }

            frame_numbers[min_frame_idx] =
                m_subfiles[roi_index][min_frame_idx]->frame_number(
                    frame_indices[min_frame_idx]);
        }
    }

    if (m_master.geometry().col == 1) {
        // get the part from each subfile and copy it to the frame
        for (size_t part_idx = 0;
             part_idx != m_ROI_geometries[roi_index].num_modules_in_roi();
             ++part_idx) {
            auto corrected_idx = frame_indices[part_idx];

            // This is where we start writing
            auto offset =
                (m_geometry
                         .get_module_geometries(
                             m_ROI_geometries[roi_index].module_indices_in_roi(
                                 part_idx))
                         .origin_y *
                     m_ROI_geometries[roi_index].pixels_x() +
                 m_geometry
                     .get_module_geometries(
                         m_ROI_geometries[roi_index].module_indices_in_roi(
                             part_idx))
                     .origin_x) *
                m_master.bitdepth() / 8;

            if (m_geometry
                    .get_module_geometries(
                        m_ROI_geometries[roi_index].module_indices_in_roi(
                            part_idx))
                    .origin_x != 0)
                throw std::runtime_error(
                    LOCATION +
                    " Implementation error. x pos not 0."); // TODO:
                                                            // origin
                                                            // can still
                                                            // change if
                                                            // roi
                                                            // changes
            // TODO! What if the files don't match?
            m_subfiles[roi_index][part_idx]->seek(corrected_idx);
            m_subfiles[roi_index][part_idx]->read_into(frame_buffer + offset,
                                                       header);
            if (header)
                ++header;
        }

    } else {
        // TODO! should we read row by row?

        // create a buffer large enough to hold a full module
        auto bytes_per_part =
            m_master.pixels_y() * m_master.pixels_x() * m_master.bitdepth() /
            8; // TODO! replace with image_size_in_bytes // TODO
               // shouldnt it only be the module size? - check

        auto *part_buffer = new std::byte[bytes_per_part];

        // TODO! if we have many submodules we should reorder them on
        // the module level

        for (size_t part_idx = 0;
             part_idx != m_ROI_geometries[roi_index].num_modules_in_roi();
             ++part_idx) {
            auto pos = m_geometry.get_module_geometries(
                m_ROI_geometries[roi_index].module_indices_in_roi(part_idx));
            auto corrected_idx = frame_indices[part_idx];

            m_subfiles[roi_index][part_idx]->seek(corrected_idx);
            m_subfiles[roi_index][part_idx]->read_into(part_buffer, header);
            if (header)
                ++header;

            for (size_t cur_row = 0; cur_row < static_cast<size_t>(pos.height);
                 cur_row++) {

                auto irow = (pos.origin_y + cur_row);
                auto icol = pos.origin_x;
                auto dest =
                    (irow * m_ROI_geometries[roi_index].pixels_x() + icol);
                dest = dest * m_master.bitdepth() / 8;
                memcpy(frame_buffer + dest,
                       part_buffer +
                           cur_row * pos.width * m_master.bitdepth() / 8,
                       pos.width * m_master.bitdepth() / 8);
            }
        }
        delete[] part_buffer;
    }
}

std::vector<Frame> RawFile::read_n(size_t n_frames) {
    // TODO: implement this in a more efficient way
    if (num_rois() > 1) {
        throw std::runtime_error(LOCATION +
                                 "Multiple ROIs defined in the master "
                                 "file. Use "
                                 "read_num_rois for a specific ROI or use "
                                 "read_ROIs to read one frame after "
                                 "the other.");
    }

    std::vector<Frame> frames;
    frames.reserve(n_frames);
    for (size_t i = 0; i < n_frames; i++) {
        frames.push_back(this->get_frame(m_current_frame));
        m_current_frame++;
    }
    return frames;
}

std::vector<Frame> RawFile::read_n_ROIs(const size_t n_frames,
                                        const size_t roi_index) {
    if (roi_index >= num_rois()) {
        throw std::runtime_error(LOCATION + "ROI index out of range.");
    }

    std::vector<Frame> frames;
    frames.reserve(n_frames);
    for (size_t i = 0; i < n_frames; i++) {
        frames.push_back(this->get_frame(m_current_frame, roi_index));
        m_current_frame++;
    }
    return frames;
}

size_t RawFile::frame_number(size_t frame_index) {
    if (frame_index >= m_master.frames_in_file()) {
        throw std::runtime_error(LOCATION + " Frame number out of range");
    }
    return m_subfiles[0][0]->frame_number(frame_index);
}

} // namespace aare
