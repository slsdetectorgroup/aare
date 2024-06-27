#include "aare/core/Dtype.hpp"
#include "aare/core/Frame.hpp"
#include "aare/core/defs.hpp"
#include <vector>

namespace aare {

namespace FrameTransformation {

Frame &identity(Frame &frame) { return (frame); }
Frame &zero(Frame &frame) {
    std::memset(frame.data(), 0, frame.size());
    return (frame);
}
Frame &flip_horizental(Frame &frame) {
    if (frame.rows() == 1) {
        return (frame);
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
    return (frame);
}

enum transformation_type { IDENTITY, ZERO, FLIP_HORIZENTAL };
std::map<transformation_type, std::function<Frame &(Frame &)>> transformations_map = {
    {IDENTITY, identity},
    {ZERO, zero},
    {FLIP_HORIZENTAL, flip_horizental},
};

class Chain {
    // vector to store the transformations function pointers
    std::vector<std::function<Frame &(Frame &)>> m_transformations;

  public:
    Chain(std::vector<transformation_type> transformations_) {
        for (auto &transformation : transformations_) {
            this->add(transformation);
        }
    }
    void add(std::function<Frame &(Frame &)> transformation) { m_transformations.push_back(transformation); }
    void add(transformation_type transformation) {
        if (transformations_map.find(transformation) == transformations_map.end()) {
            throw std::runtime_error("Transformation not found");
        }
        m_transformations.push_back(transformations_map[transformation]);
    }
    Frame &apply(Frame &frame) {
        if (m_transformations.empty()) {
            return (frame);
        }
        Frame &transformed = m_transformations[0](frame);
        for (size_t i = 1; i < m_transformations.size(); i++) {
            transformed = std::move(m_transformations[i](transformed));
        }
        return (transformed);
    }
};

} // namespace FrameTransformation
} // namespace aare