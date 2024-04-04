#include "aare/ZmqSocketSender.hpp"

#include <cassert>
#include <zmq.h>

namespace aare {
ZmqSocketSender::ZmqSocketSender(const std::string &endpoint) { m_endpoint = endpoint; }

void ZmqSocketSender::bind() {
    m_context = zmq_ctx_new();
    m_socket = zmq_socket(m_context, ZMQ_PUB);
    int rc = zmq_bind(m_socket, m_endpoint.c_str());
    assert(rc == 0);
}

int ZmqSocketSender::send(ZmqHeader &header, const std::byte *data, size_t size, bool serialize_header) {
    int rc;
    if (serialize_header) {
        rc = zmq_send(m_socket, &header, sizeof(ZmqHeader), ZMQ_SNDMORE);
        assert(rc == sizeof(ZmqHeader));
    } else {
        std::string header_str = header.to_string();
        rc = zmq_send(m_socket, header_str.c_str(), header_str.size(), ZMQ_SNDMORE);
        assert(rc == header_str.size());
    }

    int rc2 = zmq_send(m_socket, data, size, 0);
    assert(rc2 == size);
    return rc + rc2;
}

} // namespace aare