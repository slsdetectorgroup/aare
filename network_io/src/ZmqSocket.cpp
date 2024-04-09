#include "aare/network_io/ZmqSocket.hpp"
#include <zmq.h>

namespace aare {

void ZmqSocket::disconnect() {
    zmq_close(m_socket);
    zmq_ctx_destroy(m_context);
    m_socket = nullptr;
    m_context = nullptr;
}

ZmqSocket::~ZmqSocket() {
    if (m_socket)
        disconnect();
    delete[] m_header_buffer;
}

void ZmqSocket::set_zmq_hwm(int hwm) { m_zmq_hwm = hwm; }

void ZmqSocket::set_timeout_ms(int n) { m_timeout_ms = n; }

void ZmqSocket::set_potential_frame_size(size_t size) { m_potential_frame_size = size; }

} // namespace aare
