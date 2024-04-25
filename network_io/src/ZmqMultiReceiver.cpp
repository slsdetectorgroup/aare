#include "aare/network_io/ZmqMultiReceiver.hpp"
#include <zmq.h>
namespace aare {
ZmqMultiReceiver::ZmqMultiReceiver(const std::vector<std::string> &endpoints) {
    m_endpoints = endpoints;
    for (const auto &endpoint : m_endpoints) {
        m_receivers.push_back(new ZmqSingleReceiver(endpoint));
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

std::vector<ZmqFrame> ZmqMultiReceiver::receive_zmqframe() {
    std::vector<ZmqFrame> frames;
    while (1) {
        zmq_poll(items, m_receivers.size(), -1);
        logger::info("poll notified");
        for (size_t i = 0; i < m_receivers.size(); i++) {
            if (items[i].revents & ZMQ_POLLIN) {
                auto new_frame = m_receivers[i]->receive_zmqframe();
                logger::debug("ZmqFrame:", new_frame.to_string());
            }
        }
    }
    return frames;
    return {};
}

ZmqMultiReceiver::~ZmqMultiReceiver() {
    delete[] items;
    for (auto *receiver : m_receivers) {
        delete receiver;
    }
}
} // namespace aare
