#include "aare/network_io/ZmqSocketSender.hpp"
#include <cassert>
#include <zmq.h>

namespace aare {

/**
 * Constructor
 * @param endpoint ZMQ endpoint
 */
ZmqSocketSender::ZmqSocketSender(const std::string &endpoint, int socket_type) {
    m_socket_type = socket_type;
    m_endpoint = endpoint;
}

/**
 * bind to the given port
 */
void ZmqSocketSender::bind() {
    m_context = zmq_ctx_new();
    m_socket = zmq_socket(m_context, m_socket_type);
    size_t const rc = zmq_bind(m_socket, m_endpoint.c_str());
    if (rc != 0) {
        std::string const error = zmq_strerror(zmq_errno());
        throw network_io::NetworkError("zmq_bind failed: " + error);
    }
}

void ZmqSocketSender::connect() {
    m_context = zmq_ctx_new();
    m_socket = zmq_socket(m_context, m_socket_type);
    fmt::print("Setting ZMQ_RCVHWM to {}\n", m_zmq_hwm);
    int rc = zmq_setsockopt(m_socket, ZMQ_RCVHWM, &m_zmq_hwm, sizeof(m_zmq_hwm)); // should be set before connect
    if (rc)
        throw network_io::NetworkError(fmt::format("Could not set ZMQ_RCVHWM: {}", zmq_strerror(errno)));

    int bufsize = static_cast<int>(m_potential_frame_size) * m_zmq_hwm;
    fmt::print("Setting ZMQ_RCVBUF to: {} MB\n", bufsize / (static_cast<size_t>(1024) * 1024));
    rc = zmq_setsockopt(m_socket, ZMQ_RCVBUF, &bufsize, sizeof(bufsize));
    if (rc) {
        perror("zmq_setsockopt");
        throw network_io::NetworkError(fmt::format("Could not set ZMQ_RCVBUF: {}", zmq_strerror(errno)));
    }
    zmq_connect(m_socket, m_endpoint.c_str());
    zmq_setsockopt(m_socket, ZMQ_SUBSCRIBE, "", 0);
}

size_t ZmqSocketSender::send(const void *data, size_t size) {
    size_t const rc2 = zmq_send(m_socket, data, size, 0);
    assert(rc2 == size);
    return rc2;
}

/**
 * send a header and data
 * @param header
 * @param data pointer to data
 * @param size size of data
 * @return number of bytes sent
 */
size_t ZmqSocketSender::send(const ZmqHeader &header, const void *data, size_t size) {
    size_t rc = 0;
    // if (serialize_header) {
    //     rc = zmq_send(m_socket, &header, sizeof(ZmqHeader), ZMQ_SNDMORE);
    //     assert(rc == sizeof(ZmqHeader));
    std::string const header_str = header.to_string();
    aare::logger::debug("Header :", header_str);
    rc = zmq_send(m_socket, header_str.c_str(), header_str.size(), ZMQ_SNDMORE);
    assert(rc == header_str.size());
    if (data == nullptr) {
        return rc;
    }

    size_t const rc2 = zmq_send(m_socket, data, size, 0);
    assert(rc2 == size);
    return rc + rc2;
}

/**
 * Send a frame with a header
 * @param ZmqFrame that contains a header and a frame
 * @return number of bytes sent
 */
size_t ZmqSocketSender::send(const ZmqFrame &zmq_frame) {
    const Frame &frame = zmq_frame.frame;
    // send frame
    size_t const rc = send(zmq_frame.header, frame.data(), frame.size());
    // send end of message header
    ZmqHeader end_header = zmq_frame.header;
    end_header.data = false;
    size_t const rc2 = send(end_header, nullptr, 0);

    return rc + rc2;
}

/**
 * Send a vector of headers and frames
 * @param zmq_frames vector of ZmqFrame
 * @return number of bytes sent
 */
size_t ZmqSocketSender::send(const std::vector<ZmqFrame> &zmq_frames) {
    size_t rc = 0;
    for (size_t i = 0; i < zmq_frames.size(); i++) {
        const ZmqHeader &header = zmq_frames[i].header;
        const Frame &frame = zmq_frames[i].frame;
        // send header and frame
        if (i < zmq_frames.size() - 1) {
            // send header and frame
            rc += send(header, frame.data(), frame.size());
        } else {
            // send header, frame and end of message header
            rc += send({header, frame});
        }
    }
    return rc;
}
} // namespace aare