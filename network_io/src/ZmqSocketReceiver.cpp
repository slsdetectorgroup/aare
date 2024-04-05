#include "aare/ZmqSocketReceiver.hpp"
#include "aare/utils/logger.hpp"

#include <fmt/core.h>
#include <zmq.h>

namespace aare {

ZmqSocketReceiver::ZmqSocketReceiver(const std::string &endpoint) {
    m_endpoint = endpoint;
    memset(m_header_buffer, 0, m_max_header_size);
}

void ZmqSocketReceiver::connect() {
    m_context = zmq_ctx_new();
    m_socket = zmq_socket(m_context, ZMQ_SUB);
    fmt::print("Setting ZMQ_RCVHWM to {}\n", m_zmq_hwm);
    int rc = zmq_setsockopt(m_socket, ZMQ_RCVHWM, &m_zmq_hwm, sizeof(m_zmq_hwm)); // should be set before connect
    if (rc)
        throw std::runtime_error(fmt::format("Could not set ZMQ_RCVHWM: {}", strerror(errno)));

    int bufsize = m_potential_frame_size * m_zmq_hwm;
    fmt::print("Setting ZMQ_RCVBUF to: {} MB\n", bufsize / (1024 * 1024));
    rc = zmq_setsockopt(m_socket, ZMQ_RCVBUF, &bufsize, sizeof(bufsize));
    if (rc)
        throw std::runtime_error(fmt::format("Could not set ZMQ_RCVBUF: {}", strerror(errno)));

    zmq_connect(m_socket, m_endpoint.c_str());
    zmq_setsockopt(m_socket, ZMQ_SUBSCRIBE, "", 0);
}

size_t ZmqSocketReceiver::receive(ZmqHeader &header, std::byte *data, bool serialized_header) {

    size_t data_bytes_received{};

    if (serialized_header)
        throw std::runtime_error("Not implemented");

    size_t header_bytes_received = zmq_recv(m_socket, m_header_buffer, m_max_header_size, 0);

    // receive header
    m_header_buffer[header_bytes_received] = '\0'; // make sure we zero terminate
    if (header_bytes_received < 0) {
        fmt::print("Error receiving header: {}\n", strerror(errno));
        return -1;
    }
    aare::logger::debug("Bytes: ", header_bytes_received, ", Header: ", m_header_buffer);

    // parse header
    try {
        std::string header_str(m_header_buffer);
        header.from_string(header_str);
    } catch (const simdjson::simdjson_error &e) {
        aare::logger::error(LOCATION + "Error parsing header: ", e.what());
        return -1;
    }

    // do we have a multipart message (data following header)?
    int more;
    size_t more_size = sizeof(more);
    zmq_getsockopt(m_socket, ZMQ_RCVMORE, &more, &more_size);
    if (!more) {
        return 0; // no data following header
    } else {

        data_bytes_received = zmq_recv(m_socket, data, header.imageSize, 0); // TODO! configurable size!!!!
        if (data_bytes_received == -1)
            throw std::runtime_error("Got half of a multipart msg!!!");
        aare::logger::debug("Bytes: ", data_bytes_received);
    }
    return data_bytes_received + header_bytes_received;
}

} // namespace aare
