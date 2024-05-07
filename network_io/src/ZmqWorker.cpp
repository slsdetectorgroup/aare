#include "aare/network_io/ZmqWorker.hpp"
#include "aare/network_io/ZmqSocketReceiver.hpp"
#include "aare/network_io/ZmqSocketSender.hpp"
#include "aare/network_io/defs.hpp"
#include "zmq.h"
namespace aare {

ZmqWorker::ZmqWorker(const std::string &ventilator_endpoint, const std::string &sink_endpoint)
    : m_receiver(new ZmqSocketReceiver(ventilator_endpoint, ZMQ_PULL)), m_sender(nullptr) {
    m_receiver->connect();
    if (not sink_endpoint.empty()) {
        m_sender = new ZmqSocketSender(sink_endpoint, ZMQ_PUSH);
        m_sender->connect();
    }
}

Task *ZmqWorker::pull() {
    Task *task = reinterpret_cast<Task *>(new std::byte[Task::MAX_DATA_SIZE + sizeof(Task)]);
    m_receiver->receive_data(reinterpret_cast<std::byte *>(task), Task::MAX_DATA_SIZE);
    logger::debug("Received task", task->id, task->data_size);
    return task;
}

size_t ZmqWorker::push(const Task *task) {
    if (m_sender == nullptr) {
        throw network_io::NetworkError("Worker not connected to sink: did you provide a sink endpoint?");
    }
    if (task->data_size > Task::MAX_DATA_SIZE) {
        throw network_io::NetworkError("Data size exceeds maximum allowed size");
    }
    return m_sender->send(task, task->size());
}

ZmqWorker::~ZmqWorker() {
    delete m_receiver;
    delete m_sender;
}

} // namespace aare