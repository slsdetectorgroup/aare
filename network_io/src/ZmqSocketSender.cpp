#include "aare/network_io/ZmqSocketSender.hpp"
#include <cassert>
#include <zmq.h>

namespace aare {

/**
 * Constructor
 * @param endpoint ZMQ endpoint
 */
ZmqSocketSender::ZmqSocketSender(const std::string &endpoint) { m_endpoint = endpoint; }

/**
 * bind to the given port
 */
void ZmqSocketSender::bind() {
    m_context = zmq_ctx_new();
    m_socket = zmq_socket(m_context, ZMQ_PUB);
    size_t rc = zmq_bind(m_socket, m_endpoint.c_str());
    assert(rc == 0);
}

/**
 * send a header and data
 * @param header
 * @param data pointer to data
 * @param size size of data
 * @return number of bytes sent
 */
size_t ZmqSocketSender::send(const ZmqHeader &header, const std::byte *data, size_t size) {
    size_t rc;
    // if (serialize_header) {
    //     rc = zmq_send(m_socket, &header, sizeof(ZmqHeader), ZMQ_SNDMORE);
    //     assert(rc == sizeof(ZmqHeader));
    std::string header_str = header.to_string();
    rc = zmq_send(m_socket, header_str.c_str(), header_str.size(), ZMQ_SNDMORE);
    assert(rc == header_str.size());
    if (data == nullptr) {
        return rc;
    }

    size_t rc2 = zmq_send(m_socket, data, size, 0);
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
    size_t rc = send(zmq_frame.header, frame.data(), frame.size());
    // send end of message header
    ZmqHeader end_header = zmq_frame.header;
    end_header.data = false;
    size_t rc2 = send(end_header, nullptr, 0);

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