#include "aare/core/Dtype.hpp"
#include "aare/core/Frame.hpp"
#include "aare/core/defs.hpp"
#include <vector>

namespace aare {

namespace FrameTransformation {

Frame identity(const Frame &frame, bool in_place = true) { return frame; }
Frame zero(const Frame &frame, bool in_place = true) {
    if (in_place) {
        std::memset(frame.data(), 0, frame.size());
        return frame;
    }
    Frame transformed(frame);
    std::memset(transformed.data(), 0, transformed.size());
    return transformed;
}
Frame flip_horizental(const Frame &frame, bool in_place = true) {
    if (in_place) {
        if (frame.rows() == 1) {
            return frame;
        }
        std::byte *buffer = new std::byte[frame.cols() * frame.bitdepth() / 8];

        for (size_t src = 0, dst = frame.rows() - 1; src < frame.rows() / 2; dst--, src++) {
            auto src_ptr = frame.data() + src * frame.cols() * frame.bitdepth() / 8;
            auto dst_ptr = frame.data() + dst * frame.cols() * frame.bitdepth() / 8;
            std::memcpy(buffer, src_ptr, frame.cols() * frame.bitdepth() / 8);
            std::memcpy(src_ptr, dst_ptr, frame.cols() * frame.bitdepth() / 8);
            std::memcpy(dst_ptr, buffer, frame.cols() * frame.bitdepth() / 8);
        }
        delete[] buffer;
        return frame;
    } else {
        Frame transformed(frame);
        if (transformed.rows() == 1) {
            return transformed;
        }
        for (size_t src = 0, dst = frame.rows() - 1; src < transformed.rows(); src++, dst--) {
            auto src_ptr = frame.data() + src * frame.cols() * frame.bitdepth() / 8;
            auto dst_ptr = transformed.data() + dst * transformed.cols() * transformed.bitdepth() / 8;
            memcpy(dst_ptr, src_ptr, transformed.cols() * transformed.bitdepth() / 8);
        }
        return transformed;
    }
}

enum transformation_type { IDENTITY, ZERO, FLIP_HORIZENTAL };
std::map<transformation_type, std::function<Frame(const Frame &, bool)>> transformations = {
    {IDENTITY, identity},
    {ZERO, zero},
    {FLIP_HORIZENTAL, flip_horizental},
};

class Chain {
    // vector to store the transformations function pointers
    std::vector<std::function<Frame(const Frame &, bool)>> m_transformations;

  public:
    Chain(std::vector<transformation_type> transformations_) {
        for (auto &transformation : transformations_) {
            this->add(transformation);
        }
    }
    void add(std::function<Frame(const Frame &, bool)> transformation) { m_transformations.push_back(transformation); }
    void add(transformation_type transformation) {
        if (transformations.find(transformation) == transformations.end()) {
            throw std::runtime_error("Transformation not found");
        }
        m_transformations.push_back(transformations[transformation]);
    }
    Frame apply(const Frame &frame, bool in_place = true) {
        Frame transformed = frame;
        for (auto &transformation : m_transformations) {
            transformed = transformation(transformed, in_place);
        }
        return transformed;
    }
    Frame apply(const Frame &frame, std::vector<bool> in_place) {
        if (in_place.size() != m_transformations.size()) {
            throw std::runtime_error("in_place vector size should be equal to the number of transformations");
        }
        Frame transformed = frame;
        for (size_t i = 0; i < m_transformations.size(); i++) {
            transformed = m_transformations[i](transformed, in_place[i]);
        }
        return transformed;
    }
};

} // namespace FrameTransformation
} // namespace aare