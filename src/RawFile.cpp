// SPDX-License-Identifier: MPL-2.0
#include "aare/RawFile.hpp"
#include "aare/DetectorGeometry.hpp"
#include "aare/PixelMap.hpp"
#include "aare/ROI.hpp"
#include "aare/ROIGeometry.hpp"
#include "aare/algorithm.hpp"
#include "aare/defs.hpp"
#include "aare/logger.hpp"

#include <algorithm>
#include <cstring>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace aare {

std::runtime_error RawFile::frame_error(size_t frame_index,
                                        const std::string &message) const {
    return std::runtime_error(
        fmt::format("Error reading frame index {} from file '{}': {}",
                    frame_index, m_master.master_fname().string(), message));
}

RawFile::RawFile(const std::filesystem::path &fname, const std::string &mode,
                 std::optional<GapPixels> gap_pixels)
    : m_master(fname) {

    m_mode = mode;

    if (mode == "r") {
        if (m_master.frame_padding() == 0 &&
            m_master.frame_discard_policy() !=
                FrameDiscardPolicy::DiscardPartial) {
            throw std::runtime_error(fmt::format(
                "Cannot open '{}': RawFile requires frame padding or "
                "discardpartial.",
                m_master.master_fname().string()));
        }

        m_subfiles.resize(m_master.roi_geometries().size());
        // iterate over all ROIS
        const size_t num_rois = m_master.roi_geometries().size();

        for (size_t roi_index = 0; roi_index < num_rois; ++roi_index) {
            // open subfiles
            open_subfiles(roi_index);
        }

        std::optional<size_t> min_frames;
        size_t max_frames = 0;
        size_t max_part_bytes = 0;
        for (const auto &subfiles : m_subfiles) {
            for (const auto &subfile : subfiles) {
                const auto count = subfile->frames_in_file();
                min_frames = min_frames ? std::min(*min_frames, count) : count;
                max_frames = std::max(max_frames, count);
                max_part_bytes =
                    std::max(max_part_bytes, subfile->bytes_per_frame());
            }
        }
        m_part_buffer.resize(max_part_bytes);

        m_roi_rects = m_master.rois();
        if (m_roi_rects.size() != num_rois) {
            throw std::runtime_error(
                LOCATION + "ROI count differs from ROI geometry count");
        }
        m_roi_shapes.reserve(num_rois);
        if (gap_pixels) {
            m_gap_pixels = gap_pixels;
            m_gap_layout = detail::gap_layout(
                m_master.detector_type(), m_master.quad() == 1, *gap_pixels);
            m_generator = detail::make_generator(gap_pixels->seed);
            for (const auto &rect : m_roi_rects) {
                m_roi_shapes.push_back(
                    detail::gapped_shape(rect, m_gap_layout));
            }
        } else {
            for (const auto &roi : m_master.roi_geometries()) {
                m_roi_shapes.push_back({static_cast<ssize_t>(roi.pixels_y()),
                                        static_cast<ssize_t>(roi.pixels_x())});
            }
        }
        if (!min_frames) {
            throw std::runtime_error(fmt::format(
                "No raw subfiles selected by '{}'", fname.string()));
        }
        m_frames_in_file = *min_frames;

        if (m_frames_in_file != max_frames ||
            m_frames_in_file != m_master.frames_in_file()) {
            LOG(logWARNING) << fmt::format(
                "'{}': Different number of frames across subfiles. Expected {} "
                "frames but found min/max {}/{}, "
                "using {} frames.",
                fname.string(), m_master.frames_in_file(), m_frames_in_file,
                max_frames, m_frames_in_file);
        }
        LOG(logDEBUG) << "Frames in file: " << m_frames_in_file;
    } else {
        throw std::runtime_error(LOCATION +
                                 " Unsupported mode. Can only read RawFiles.");
    }
}

Frame RawFile::read_roi(const size_t roi_index) {

    if (roi_index >= m_master.roi_geometries().size()) {
        throw frame_error(m_current_frame,
                          LOCATION + "ROI index out of range.");
    }
    return get_frame(m_current_frame++, roi_index);
}

std::vector<Frame> RawFile::read_rois() {
    const size_t num_rois = m_master.roi_geometries().size();

    std::vector<Frame> frames;
    frames.reserve(num_rois);

    for (size_t roi_idx = 0; roi_idx < num_rois; ++roi_idx) {
        frames.push_back(get_frame(m_current_frame, roi_idx));
    }
    ++m_current_frame;

    return frames;
}

Frame RawFile::read_frame() {
    if (m_master.roi_geometries().size() > 1) {
        throw frame_error(m_current_frame, LOCATION +
                                               "Multiple ROIs present in file. "
                                               "Use read_ROIs() instead.");
    }
    return get_frame(m_current_frame++);
}

Frame RawFile::read_frame(size_t frame_number) {
    if (m_master.roi_geometries().size() > 1) {
        throw frame_error(
            frame_number,
            LOCATION + "Multiple ROIs present in file. "
                       "Use read_ROIs(const size_t frame_number) instead.");
    }
    seek(frame_number);
    return read_frame();
}

void RawFile::read_into(std::byte *image_buf, size_t n_frames) {
    // TODO: implement this in a more efficient way
    if (m_master.roi_geometries().size() > 1) {
        throw frame_error(m_current_frame,
                          LOCATION + "Cannot use read_into for multiple ROIs.");
    }

    for (size_t i = 0; i < n_frames; i++) {
        this->get_frame_into(m_current_frame++, image_buf);
        image_buf += bytes_per_frame();
    }
}

void RawFile::read_into(std::byte *image_buf) {
    if (m_master.roi_geometries().size() > 1) {
        throw frame_error(m_current_frame,
                          LOCATION +
                              "Cannot use read_into for multiple ROIs. Use "
                              "read_roi_into() for a single ROI instead.");
    }
    return get_frame_into(m_current_frame++, image_buf);
}

void RawFile::read_roi_into(std::byte *image_buf, const size_t roi_index,
                            const size_t frame_number, DetectorHeader *header) {
    if (roi_index >= num_rois()) {
        throw frame_error(frame_number, LOCATION + "ROI index out of range.");
    }
    return get_frame_into(frame_number, image_buf, roi_index, header);
}

void RawFile::read_into(std::byte *image_buf, DetectorHeader *header) {
    if (m_master.roi_geometries().size() > 1) {
        throw frame_error(m_current_frame,
                          LOCATION +
                              "Cannot use read_into for multiple ROIs. Use "
                              "read_roi_into() for a single ROI instead.");
    }
    return get_frame_into(m_current_frame++, image_buf, 0, header);
}

void RawFile::read_into(std::byte *image_buf, size_t n_frames,
                        DetectorHeader *header) {
    // return get_frame_into(m_current_frame++, image_buf, header);

    if (m_master.roi_geometries().size() > 1) {
        throw frame_error(
            m_current_frame,
            LOCATION +
                "Cannot use read_into for multiple ROIs."); // TODO: maybe
                                                            // pass
                                                            // roi_index so
                                                            // one can use
                                                            // read_into for
                                                            // a specific
                                                            // ROI
    }

    for (size_t i = 0; i < n_frames; i++) {
        this->get_frame_into(m_current_frame++, image_buf, 0, header);
        image_buf += bytes_per_frame();
        if (header)
            header += m_master.roi_geometries()[0].num_modules_in_roi();
    }
}

size_t RawFile::bytes_per_frame() {
    if (m_master.roi_geometries().size() > 1) {
        throw std::runtime_error(
            LOCATION + "Pass the desired roi_index to bytes_per_frame to get "
                       "bytes_per_frame for the specific ROI. ");
    }
    return bytes_per_frame(0);
}

size_t RawFile::bytes_per_frame(const size_t roi_index) {
    return pixels_per_frame(roi_index) * m_master.bitdepth() / bits_per_byte;
}

size_t RawFile::pixels_per_frame() {
    if (m_master.roi_geometries().size() > 1) {
        throw std::runtime_error(
            LOCATION + "Pass the desired roi_index to pixels_per_frame to get "
                       "pixels_per_frame for the specific ROI. ");
    }
    return pixels_per_frame(0);
}

size_t RawFile::pixels_per_frame(const size_t roi_index) {
    return rows(roi_index) * cols(roi_index);
}

DetectorType RawFile::detector_type() const { return m_master.detector_type(); }

void RawFile::seek(size_t frame_index) {
    // check if the frame number is greater than the total frames
    // if frame_number == total_frames, then the next read will throw an
    // error
    if (frame_index > total_frames()) {
        throw frame_error(
            frame_index,
            fmt::format("frame number {} is greater than total frames {}",
                        frame_index, total_frames()));
    }
    m_current_frame = frame_index;
}

size_t RawFile::tell() { return m_current_frame; }

size_t RawFile::total_frames() const { return m_frames_in_file; }

size_t RawFile::rows() const {
    if (m_master.roi_geometries().size() > 1) {
        throw std::runtime_error(LOCATION +
                                 "Pass the desired roi_index to rows to get "
                                 "rows for the specific ROI. ");
    }
    return rows(0);
}
size_t RawFile::rows(const size_t roi_index) const {
    return static_cast<size_t>(m_roi_shapes.at(roi_index)[0]);
}
size_t RawFile::cols() const {
    if (m_master.roi_geometries().size() > 1) {
        throw std::runtime_error(LOCATION +
                                 "Pass the desired roi_index to cols to get "
                                 "cols for the specific ROI. ");
    }
    return cols(0);
}
size_t RawFile::cols(const size_t roi_index) const {
    return static_cast<size_t>(m_roi_shapes.at(roi_index)[1]);
}
size_t RawFile::bitdepth() const { return m_master.bitdepth(); }

xy RawFile::geometry() const { return m_master.detector_layout(); }

size_t RawFile::n_modules() const { return m_master.n_modules(); };

size_t RawFile::num_rois() const { return m_master.roi_geometries().size(); }

const ROIGeometry &RawFile::roi_geometries(size_t roi_index) const {
    return m_master.roi_geometries().at(roi_index);
}

std::vector<size_t> RawFile::n_modules_in_roi() const {

    std::vector<size_t> results(m_master.roi_geometries().size());
    std::transform(
        m_master.roi_geometries().begin(), m_master.roi_geometries().end(),
        results.begin(),
        [](const ROIGeometry &roi) { return roi.num_modules_in_roi(); });
    return results;
}

void RawFile::open_subfiles(const size_t roi_index) {
    if (m_mode == "r") {

        m_subfiles[roi_index].reserve(
            m_master.roi_geometries().at(roi_index).num_modules_in_roi());

        auto module_indices =
            m_master.roi_geometries().at(roi_index).module_indices_in_roi();

        for (const size_t i : module_indices) {
            const auto pos = m_master.geometry().get_module_geometries(i);
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
    FILE *fp = fopen(fname.string().c_str(), "rb");
    if (!fp)
        throw std::runtime_error(fmt::format(
            "Could not open file '{}' for frame index 0", fname.string()));

    size_t const rc = fread(reinterpret_cast<char *>(&h), sizeof(h), 1, fp);
    if (rc != 1) {
        fclose(fp);
        throw std::runtime_error(
            LOCATION + fmt::format("Could not read header for frame index 0 "
                                   "from file '{}'",
                                   fname.string()));
    }
    if (fclose(fp)) {
        throw std::runtime_error(
            LOCATION + fmt::format("Could not close file '{}' after reading "
                                   "frame index 0",
                                   fname.string()));
    }

    return h;
}

RawMasterFile RawFile::master() const { return m_master; }

Frame RawFile::get_frame(size_t frame_index, const size_t roi_index) {
    auto f = Frame(static_cast<uint32_t>(rows(roi_index)),
                   static_cast<uint32_t>(cols(roi_index)),
                   Dtype::from_bitdepth(m_master.bitdepth()));
    std::byte *frame_buffer = f.data();
    get_frame_into(frame_index, frame_buffer, roi_index);
    return f;
}

size_t RawFile::bytes_per_pixel() const { return m_master.bitdepth() / 8; }

std::vector<size_t> RawFile::synchronized_frame_indices(size_t frame_index,
                                                        size_t roi_index) {
    const auto &roi = m_master.roi_geometries().at(roi_index);
    const size_t n_parts = roi.num_modules_in_roi();
    std::vector<size_t> frame_indices(n_parts, frame_index);
    if (n_parts == 1) {
        return frame_indices;
    }

    std::vector<size_t> frame_numbers(n_parts);
    for (size_t part_idx = 0; part_idx != n_parts; ++part_idx) {
        frame_numbers[part_idx] =
            m_subfiles[roi_index][part_idx]->frame_number(frame_index);
    }

    while (!all_equal(frame_numbers)) {
        // advance the part with the lowest frame number until all agree
        const auto min_frame_idx = static_cast<size_t>(std::distance(
            frame_numbers.begin(),
            std::min_element(frame_numbers.begin(), frame_numbers.end())));

        frame_indices[min_frame_idx]++;
        if (frame_indices[min_frame_idx] >= total_frames()) {
            throw frame_error(
                frame_index,
                LOCATION +
                    fmt::format(
                        "Frame index {} out of range while synchronizing "
                        "module {} for ROI {}; last data file '{}'",
                        frame_indices[min_frame_idx], min_frame_idx, roi_index,
                        m_subfiles[roi_index][min_frame_idx]
                            ->current_path()
                            .string()));
        }
        frame_numbers[min_frame_idx] =
            m_subfiles[roi_index][min_frame_idx]->frame_number(
                frame_indices[min_frame_idx]);
    }
    return frame_indices;
}

void RawFile::get_frame_into(size_t frame_index, std::byte *frame_buffer,
                             const size_t roi_index, DetectorHeader *header) {
    LOG(logDEBUG) << "RawFile::get_frame_into(" << frame_index << ")";
    if (frame_index >= total_frames()) {
        throw frame_error(
            frame_index,
            LOCATION +
                fmt::format("Frame index {} is out of range: file contains {} "
                            "frames (indices are zero-based)",
                            frame_index, total_frames()));
    }

    const auto &roi = m_master.roi_geometries().at(roi_index);
    const auto frame_indices =
        synchronized_frame_indices(frame_index, roi_index);

    if (m_gap_pixels) {
        switch (m_master.bitdepth()) {
        case 8:
            assemble_with_gaps<uint8_t>(frame_buffer, roi_index, frame_indices,
                                        header);
            break;
        case 16:
            assemble_with_gaps<uint16_t>(frame_buffer, roi_index, frame_indices,
                                         header);
            break;
        case 32:
            assemble_with_gaps<uint32_t>(frame_buffer, roi_index, frame_indices,
                                         header);
            break;
        default:
            throw frame_error(frame_index,
                              LOCATION +
                                  fmt::format("Gap pixels are not supported "
                                              "for a bit depth of {}",
                                              m_master.bitdepth()));
        }
        return;
    }

    const size_t bytes_per_pixel = m_master.bitdepth() / bits_per_byte;
    const size_t roi_width = roi.pixels_x();

    for (size_t part_idx = 0; part_idx != roi.num_modules_in_roi();
         ++part_idx) {
        // origin and size of the part relative to the ROI
        const auto &pos = m_master.geometry().get_module_geometries(
            roi.module_indices_in_roi(part_idx));
        const auto width = static_cast<size_t>(pos.width);
        const auto height = static_cast<size_t>(pos.height);
        auto &subfile = *m_subfiles[roi_index][part_idx];
        subfile.seek(frame_indices[part_idx]);

        std::byte *dest =
            frame_buffer + (static_cast<size_t>(pos.origin_y) * roi_width +
                            static_cast<size_t>(pos.origin_x)) *
                               bytes_per_pixel;

        if (width == roi_width) {
            // the part spans full rows of the ROI and is contiguous in the
            // assembled frame
            subfile.read_into(dest, header);
        } else {
            subfile.read_into(m_part_buffer.data(), header);
            const size_t row_bytes = width * bytes_per_pixel;
            const size_t roi_row_bytes = roi_width * bytes_per_pixel;
            for (size_t row = 0; row < height; ++row) {
                std::memcpy(dest + row * roi_row_bytes,
                            m_part_buffer.data() + row * row_bytes, row_bytes);
            }
        }
        if (header) {
            ++header;
        }
    }
}

template <typename T>
void RawFile::assemble_with_gaps(std::byte *frame_buffer, size_t roi_index,
                                 const std::vector<size_t> &frame_indices,
                                 DetectorHeader *header) {
    const auto &roi = m_master.roi_geometries().at(roi_index);
    const ROI &rect = m_roi_rects.at(roi_index);
    NDView<T, 2> destination(reinterpret_cast<T *>(frame_buffer),
                             m_roi_shapes.at(roi_index));
    detail::fill_gap_pixels(destination, rect, m_gap_layout,
                            detail::fill_as<T>(m_gap_pixels->fill_value));

    for (size_t part_idx = 0; part_idx != roi.num_modules_in_roi();
         ++part_idx) {
        const auto &pos = m_master.geometry().get_module_geometries(
            roi.module_indices_in_roi(part_idx));
        auto &subfile = *m_subfiles[roi_index][part_idx];
        subfile.seek(frame_indices[part_idx]);
        subfile.read_into(m_part_buffer.data(), header);
        if (header) {
            ++header;
        }
        // detector coordinates of the part
        const ROI part_rect{
            rect.xmin + pos.origin_x, rect.xmin + pos.origin_x + pos.width,
            rect.ymin + pos.origin_y, rect.ymin + pos.origin_y + pos.height};
        NDView<const T, 2> source(
            reinterpret_cast<const T *>(m_part_buffer.data()),
            {pos.height, pos.width});
        detail::copy_with_gaps(source, part_rect, destination, rect,
                               m_gap_layout);
    }

    if (m_gap_pixels->split_counts) {
        detail::split_edge_counts(destination, rect, m_gap_layout, m_generator);
    }
}

std::vector<Frame> RawFile::read_n(size_t n_frames) {
    // TODO: implement this in a more efficient way
    if (num_rois() > 1) {
        throw frame_error(m_current_frame,
                          LOCATION + "Multiple ROIs defined in the master "
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

std::vector<Frame> RawFile::read_n_with_roi(const size_t n_frames,
                                            const size_t roi_index) {
    if (roi_index >= num_rois()) {
        throw frame_error(m_current_frame,
                          LOCATION + "ROI index out of range.");
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
    if (frame_index >= total_frames()) {
        throw frame_error(frame_index, "Frame number out of range");
    }
    return m_subfiles[0][0]->frame_number(frame_index);
}

} // namespace aare
