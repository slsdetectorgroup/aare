#include "aare/network_io/ZmqMultiReceiver.hpp"
#include "aare/utils/merge_frames.hpp"
#include <unordered_map>
#include <zmq.h>
namespace aare {
ZmqMultiReceiver::ZmqMultiReceiver(const std::vector<std::string> &endpoints, const xy &geometry)
    : m_geometry(geometry), m_endpoints(endpoints) {
    assert(m_geometry.row * m_geometry.col == static_cast<uint32_t>(m_endpoints.size()));
    for (const auto &endpoint : m_endpoints) {
        m_receivers.push_back(new ZmqSocketReceiver(endpoint));
    }
}

int ZmqMultiReceiver::connect() {
    for (auto *receiver : m_receivers) {
        receiver->connect();
    }
    items = new zmq_pollitem_t[m_receivers.size()];
    for (size_t i = 0; i < m_receivers.size(); i++) {
        items[i] = {m_receivers[i]->get_socket(), 0, ZMQ_POLLIN, 0};
    }
    return 0;
}
ZmqFrame ZmqMultiReceiver::receive_zmqframe() {
    std::unordered_map<uint64_t, std::vector<ZmqFrame>> frames_map;
    return receive_zmqframe_(frames_map);
}
std::vector<ZmqFrame> ZmqMultiReceiver::receive_n() {
    std::vector<ZmqFrame> frames;
    std::unordered_map<uint64_t, std::vector<ZmqFrame>> frames_map;
    while (true) {
        // receive header and frame
        ZmqFrame const zmq_frame = receive_zmqframe_(frames_map);
        if (!zmq_frame.header.data) {
            break;
        }
        frames.push_back(zmq_frame);
        frames_map.erase(zmq_frame.header.frameNumber);
    }
    return frames;
}
ZmqFrame ZmqMultiReceiver::receive_zmqframe_(std::unordered_map<uint64_t, std::vector<ZmqFrame>> &frames_map) {
    // iterator to store the frame to return
    std::unordered_map<uint64_t, std::vector<ZmqFrame>>::iterator ret_frames;
    bool exit_loop = false;

    while (true) {
        zmq_poll(items, static_cast<int>(m_receivers.size()), -1);
        aare::logger::debug("Received frame");
        for (size_t i = 0; i < m_receivers.size() && !exit_loop; i++) {
            if (items[i].revents & ZMQ_POLLIN) {
                auto new_frame = m_receivers[i]->receive_zmqframe();
                if (frames_map.find(new_frame.header.frameNumber) == frames_map.end()) {
                    frames_map[new_frame.header.frameNumber] = {};
                }

                ret_frames = frames_map.find(new_frame.header.frameNumber);
                ret_frames->second.push_back(new_frame);

                exit_loop = ret_frames->second.size() == m_receivers.size();
            }
        }
        if (exit_loop) {
            break;
        }
    }
    std::vector<ZmqFrame> &frames = ret_frames->second;
    if (!frames[0].header.data) {
        return ZmqFrame{frames[0].header, Frame(0, 0, 0)};
    }
    // check that all frames have the same shape
    auto shape = frames[0].header.shape;
    auto bitdepth = frames[0].header.bitmode;
    auto part_size = shape.row * shape.col * (bitdepth / 8);
    for (auto &frame : frames) {
        assert(shape == frame.header.shape);
        assert(bitdepth == frame.header.bitmode);
        // TODO: find solution for numinterfaces=2
        assert(m_geometry == frame.header.detshape);
        assert(part_size == frame.header.size);
    }
    // merge frames
    // prepare the input for merge_frames
    std::vector<std::byte *> part_buffers;
    part_buffers.reserve(frames.size());
    for (auto &zmq_frame : frames) {
        part_buffers.push_back(zmq_frame.frame.data());
    }
    Frame const f(shape.row, shape.col, bitdepth);
    merge_frames(part_buffers, part_size, f.data(), m_geometry, shape.row, shape.col, bitdepth);
    ZmqFrame zmq_frame = {std::move(frames[0].header), f};
    return zmq_frame;
}

ZmqMultiReceiver::~ZmqMultiReceiver() {
    delete[] items;
    for (auto *receiver : m_receivers) {
        delete receiver;
    }
}
} // namespace aare
