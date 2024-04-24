#include "ZmqMultiReceiver.hpp"

namespace aare {
ZmqMultiReceiver::ZmqMultiReceiver(const std::vector<std::string> &endpoints) {
    m_endpoints = endpoints;
    for (const auto &endpoint : m_endpoints) {
        m_receivers.push_back(ZmqSingleReceiver(endpoint));
    }
}

int ZmqMultiReceiver::connect() {
    for (auto &receiver : m_receivers) {
        receiver.connect();
    }
    items = new zmq_pollitem_t[m_receivers.size()];
    for (size_t i = 0; i < m_receivers.size(); i++) {
        items[i] = {m_receivers[i].get_socket(), 0, ZMQ_POLLIN, 0};
    }
    zmq_poll(items, m_receivers.size(), -1);
    logger::info("Receivers polled");
}

std::vector<ZmqFrame> ZmqMultiReceiver::receive_zmqframe() {
    std::vector<ZmqFrame> frames;
    while (1) {
        for (size_t i = 0; i < m_receivers.size(); i++) {
            if (items[i].revents & ZMQ_POLLIN) {
                 new_frames = m_receivers[i].receive_zmqframe();
                frames.insert(frames.end(), new_frames.begin(), new_frames.end());
            }
        }
    }
    return frames;
}

ZmqMultiReceiver::~ZmqMultiReceiver() { delete[] items; }
} // namespace aare
