#include "aare/network_io/ZmqVentilator.hpp"
#include <cassert>
#include <zmq.h>

namespace aare {

ZmqVentilator::ZmqVentilator(const std::string &endpoint) : m_sender(new ZmqSocketSender(endpoint, ZMQ_PUSH)) {
    m_sender->bind();
}

size_t ZmqVentilator::push(const Task *task) {
    if (task->data_size > Task::MAX_DATA_SIZE) {
        throw network_io::NetworkError("Data size exceeds maximum allowed size");
    }
    logger::debug("Pushing workers");
    return m_sender->send(task, task->size());
}

ZmqVentilator::~ZmqVentilator() { delete m_sender; }

} // namespace aare