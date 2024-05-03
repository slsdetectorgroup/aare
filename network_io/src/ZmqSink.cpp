
#include "aare/network_io/ZmqSink.hpp"
#include "zmq.h"

namespace aare {

ZmqSink::ZmqSink(const std::string &sink_endpoint) : m_receiver(new ZmqSingleReceiver(sink_endpoint, ZMQ_PULL)) {
    m_receiver->bind();
};

Task *ZmqSink::pull() {
    Task *task = reinterpret_cast<Task *>(new std::byte[Task::MAX_DATA_SIZE + sizeof(Task)]);
    m_receiver->receive_data(reinterpret_cast<std::byte *>(task), Task::MAX_DATA_SIZE);
    return task;
};

ZmqSink::~ZmqSink() { delete m_receiver; };

} // namespace aare