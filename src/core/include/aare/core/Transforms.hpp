#pragma once
#include "aare/core/Dtype.hpp"
#include "aare/core/Frame.hpp"
#include "aare/core/defs.hpp"
#include <unordered_set>
#include <vector>

namespace aare {

class Transforms {
    // vector to store the transformations function pointers
    std::vector<std::function<Frame &(Frame &)>> m_transformations{};

  public:
    Transforms() = default;
    Transforms(std::vector<std::function<Frame &(Frame &)>> transformations_) : m_transformations(transformations_) {}
    void add(std::function<Frame &(Frame &)> transformation) { m_transformations.push_back(transformation); }
    void add(std::vector<std::function<Frame &(Frame &)>> transformations) {
        for (auto &transformation : transformations) {
            this->add(transformation);
        }
    }

    Frame &apply(Frame &frame) {
        for (auto &transformation : m_transformations) {
            transformation(frame);
        }
        return frame;
    }
    Frame &operator()(Frame &frame) { return apply(frame); }

    static std::function<Frame &(Frame &)> identity() {
        return [](Frame &frame) -> Frame & { return frame; };
    }

    static std::function<Frame &(Frame &)> zero() {
        return [](Frame &frame) -> Frame & {
            std::memset(frame.data(), 0, frame.bytes());
            return frame;
        };
    }

    static std::function<Frame &(Frame &)> flip_horizental() {
        return [](Frame &frame) -> Frame & {
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
        };
    }

    static std::function<Frame &(Frame &)> reorder(NDView<size_t, 2> &order_map) {
        if (order_map.size() == 0) {
            throw std::runtime_error("Order map is empty");
        }
        // verify that the order map doesn't have duplicates
        std::unordered_set<size_t> seen(order_map.begin(), order_map.end());
        if (seen.size() != order_map.size()) {
            throw std::runtime_error("Order map has duplicates");
        }
        // verify that the order map has all the values from 0 to rows * cols
        for (size_t i = 0; i < order_map.size(); i++) {
            if (order_map[i] >= order_map.size() || order_map[i] < 0) {
                throw std::runtime_error("Order map has values less than 0 or greater than rows * cols");
            }
        }
        return [order_map](Frame &frame) -> Frame & {
            // verify that the order map has the same shape as the frame
            if (order_map.shape(0) != frame.rows() || order_map.shape(1) != frame.cols()) {
                throw std::runtime_error("Order map shape mismatch");
            }

            Frame frame_copy = frame.copy();

            // prepare variable for performance optimization (not tested)
            const size_t pixel_depth = frame.bitdepth() / 8;
            std::byte *const dst_data = frame.data();
            std::byte *const src_data = frame_copy.data();
            const size_t size = order_map.size();
            size_t idx = 0;
            size_t new_idx;
            std::byte *src;
            std::byte *dst;
            // reorder the frame
            for (; idx < size; idx++) {
                new_idx = order_map[idx];
                src = src_data + idx * pixel_depth;
                dst = dst_data + new_idx * pixel_depth;
                std::memcpy(dst, src, pixel_depth);
            };
            return frame;
        };
    }
    static std::function<Frame &(Frame &)> reorder(std::vector<size_t> &order_map) {
        NDView<size_t, 2> order_map_view(order_map.data(), {order_map.size(), 1});
        return reorder(order_map_view);
    }
    static std::function<Frame &(Frame &)> reorder(NDArray<size_t, 2> &order_map) {
        NDView<size_t, 2> order_map_view(order_map.data(), order_map.shape());
        return reorder(order_map_view);
    }
};

} // namespace aare