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
    static NDArray<uint64_t, 2> MOENCH_ORDER_MAP;
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

            for (uint64_t src = 0, dst = frame.rows() - 1; src < frame.rows() / 2; dst--, src++) {
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

    static std::function<Frame &(Frame &)> reorder(NDView<uint64_t, 2> &order_map) {
        if (order_map.size() == 0) {
            throw std::runtime_error("Order map is empty");
        }
        // verify that the order map doesn't have duplicates
        std::unordered_set<uint64_t> seen(order_map.begin(), order_map.end());
        if (seen.size() != order_map.size()) {
            throw std::runtime_error("Order map has duplicates");
        }
        // verify that the order map has all the values from 0 to rows * cols
        for (uint64_t i = 0; i < order_map.size(); i++) {
            if (order_map[i] >= order_map.size()) {
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
            const uint64_t pixel_depth = frame.bitdepth() / 8;
            std::byte *const dst_data = frame.data();
            std::byte *const src_data = frame_copy.data();
            const uint64_t size = order_map.size();
            uint64_t idx = 0;
            uint64_t new_idx;
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
    static std::function<Frame &(Frame &)> reorder(std::vector<uint64_t> &order_map) {
        int64_t tmp = static_cast<int64_t>(order_map.size());
        NDView<uint64_t, 2> order_map_view(order_map.data(), {tmp, 1});
        return reorder(order_map_view);
    }
    static std::function<Frame &(Frame &)> reorder(NDArray<uint64_t, 2> &order_map) {
        NDView<uint64_t, 2> order_map_view(order_map.data(), order_map.shape());
        return reorder(order_map_view);
    }
    static NDArray<uint64_t, 2> generate_moench_order_map() {
        std::array<int, 32> const adc_nr = {300, 325, 350, 375, 300, 325, 350, 375, 200, 225, 250,
                                            275, 200, 225, 250, 275, 100, 125, 150, 175, 100, 125,
                                            150, 175, 0,   25,  50,  75,  0,   25,  50,  75};
        int const sc_width = 25;
        int const nadc = 32;
        int const pixels_per_sc = 5000;
        NDArray<uint64_t, 2> order_map({400, 400});

        int pixel = 0;
        for (int i = 0; i != pixels_per_sc; ++i) {
            for (int i_adc = 0; i_adc != nadc; ++i_adc) {
                int const col = adc_nr[i_adc] + (i % sc_width);
                int row = 0;
                if ((i_adc / 4) % 2 == 0)
                    row = 199 - (i / sc_width);
                else
                    row = 200 + (i / sc_width);

                order_map(row, col) = pixel;
                pixel++;
            }
        }
        return order_map;
    }

    static std::function<Frame &(Frame &)> reorder_moench() {
        return reorder(MOENCH_ORDER_MAP);
    }
};

NDArray<uint64_t,2> Transforms::MOENCH_ORDER_MAP = Transforms::generate_moench_order_map();

} // namespace aare