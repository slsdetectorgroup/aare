
#include "aare/network_io/ZmqSink.hpp"
#include "zmq.h"

namespace aare {

ZmqSink::ZmqSink(const std::string &ventilator_endpoint) {
    m_receiver = new ZmqSingleReceiver(ventilator_endpoint, ZMQ_PULL);
    m_receiver->bind();
};

Task *ZmqSink::pull() {
    Task *task = (Task *)new std::byte[Task::MAX_DATA_SIZE + sizeof(Task)];
    m_receiver->receive_data((std::byte *)task, Task::MAX_DATA_SIZE);
    return task;
};

ZmqSink::~ZmqSink() { delete m_receiver; };

} // namespace aare