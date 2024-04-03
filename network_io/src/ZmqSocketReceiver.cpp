#include "aare/ZmqSocketReceiver.hpp"
#include <fmt/core.h>
#include <zmq.h>

namespace aare {

ZmqSocketReceiver::ZmqSocketReceiver(const std::string &endpoint) : m_endpoint(endpoint) {
    memset(m_header_buffer, 0, m_max_header_size);
}

void ZmqSocketReceiver::connect() {
    m_context = zmq_ctx_new();
    m_socket = zmq_socket(m_context, ZMQ_SUB);
    fmt::print("Setting ZMQ_RCVHWM to {}\n", m_zmq_hwm);
    int rc = zmq_setsockopt(m_socket, ZMQ_RCVHWM, &m_zmq_hwm, sizeof(m_zmq_hwm)); // should be set before connect
    if (rc)
        throw std::runtime_error(fmt::format("Could not set ZMQ_RCVHWM: {}", strerror(errno)));

    int bufsize = 1024 * 1024 * m_zmq_hwm;
    fmt::print("Setting ZMQ_RCVBUF to: {} MB\n", bufsize / (1024 * 1024));
    rc = zmq_setsockopt(m_socket, ZMQ_RCVBUF, &bufsize, sizeof(bufsize));
    if (rc)
        throw std::runtime_error(fmt::format("Could not set ZMQ_RCVBUF: {}", strerror(errno)));

    zmq_connect(m_socket, m_endpoint.c_str());
    zmq_setsockopt(m_socket, ZMQ_SUBSCRIBE, "", 0);
}

void ZmqSocketReceiver::disconnect() {
    zmq_close(m_socket);
    zmq_ctx_destroy(m_context);
    m_socket = nullptr;
    m_context = nullptr;
}

ZmqSocketReceiver::~ZmqSocketReceiver() {
    if (m_socket)
        disconnect();
    delete[] m_header_buffer;
}

void ZmqSocketReceiver::set_zmq_hwm(int hwm) { m_zmq_hwm = hwm; }

void ZmqSocketReceiver::set_timeout_ms(int n) { m_timeout_ms = n; }

int ZmqSocketReceiver::receive(zmqHeader &header, std::byte *data) {

    // receive header
    int header_bytes_received = zmq_recv(m_socket, m_header_buffer, m_max_header_size, 0);
    m_header_buffer[header_bytes_received] = '\0'; // make sure we zero terminate
    if (header_bytes_received < 0) {
        fmt::print("Error receiving header: {}\n", strerror(errno));
        return -1;
    }
    fmt::print("Bytes: {}, Header: {}\n", header_bytes_received, m_header_buffer);

    // decode header
    if (!decode_header(header)) {
        fmt::print("Error decoding header\n");
        return -1;
    }

    // do we have a multipart message (data following header)?
    int more;
    size_t more_size = sizeof(more);
    zmq_getsockopt(m_socket, ZMQ_RCVMORE, &more, &more_size);
    if (!more) {
        return 0; // no data following header
    } else {
        int data_bytes_received = zmq_recv(m_socket, data, 1024 * 1024 * 2, 0); // TODO! configurable size!!!!
        if (data_bytes_received == -1)
            throw std::runtime_error("Got half of a multipart msg!!!");
    }
    return 1;
}

bool ZmqSocketReceiver::decode_header(zmqHeader &h) {
    // TODO: implement
    return true;
}

} // namespace aare
