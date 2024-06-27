#include "aare/network_io/ZmqSocketReceiver.hpp"
#include "aare/utils/logger.hpp"

#include <fmt/core.h>
#include <zmq.h>

namespace aare {

/**
 * @brief Construct a new ZmqSocketReceiver object
 */
ZmqSocketReceiver::ZmqSocketReceiver(const std::string &endpoint, int socket_type) {
    m_endpoint = (endpoint);
    m_socket_type = (socket_type);
    memset(m_header_buffer, 0, m_max_header_size);
}

/**
 * @brief Connect to the given endpoint
 * subscribe to a Zmq published
 */
void ZmqSocketReceiver::connect() {
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

void ZmqSocketReceiver::bind() {
    m_context = zmq_ctx_new();
    m_socket = zmq_socket(m_context, m_socket_type);
    size_t const rc = zmq_bind(m_socket, m_endpoint.c_str());
    if (rc != 0) {
        std::string const error = zmq_strerror(zmq_errno());
        throw network_io::NetworkError("zmq_bind failed: " + error);
    }
}

/**
 * @brief receive a ZmqHeader
 * @return ZmqHeader
 */
ZmqHeader ZmqSocketReceiver::receive_header() {

    // receive string ZmqHeader
    int const header_bytes_received = zmq_recv(m_socket, m_header_buffer, m_max_header_size, 0);
    aare::logger::debug("Header: ", m_header_buffer);

    m_header_buffer[header_bytes_received] = '\0'; // make sure we zero terminate
    if (header_bytes_received < 0) {
        throw network_io::NetworkError(LOCATION + "Error receiving header");
    }

    // parse header
    ZmqHeader header;
    try {
        std::string header_str(m_header_buffer);
        header.from_string(header_str);
    } catch (const simdjson::simdjson_error &e) {
        throw network_io::NetworkError(LOCATION + "Error parsing header: " + e.what());
    }
    return header;
}

/**
 * @brief receive data following a ZmqHeader
 * @param data pointer to data
 * @param size size of data
 * @return ZmqHeader
 */
int ZmqSocketReceiver::receive_data(std::byte *data, size_t size) {
    int const data_bytes_received = zmq_recv(m_socket, data, size, 0);
    if (data_bytes_received == -1) {
        logger::error(zmq_strerror(zmq_errno()));
        // TODO: refactor this error message
        throw network_io::NetworkError(LOCATION + "Error receiving data");
    }
    // aare::logger::debug("Bytes: ", data_bytes_received);

    return data_bytes_received;
}

/**
 * @brief receive a ZmqFrame (header and data)
 * @return ZmqFrame
 */
ZmqFrame ZmqSocketReceiver::receive_zmqframe() {
    // receive header from zmq and parse it
    ZmqHeader header = receive_header();

    if (!header.data) {
        // no data following header
        Frame frame(0, 0, Dtype::NONE);
        return {header, frame};
    }

    // receive frame data
    if (header.shape == t_xy<uint32_t>{0, 0} || header.bitmode == 0) {
        logger::warn("Invalid header");
    }
    if (header.bitmode == 0) {
        header.bitmode = 16;
    }
    Frame frame(header.shape.row, header.shape.col, Dtype::from_bitdepth(header.bitmode));
    int bytes_received = receive_data(frame.data(), frame.size());
    if (bytes_received == -1) {
        throw network_io::NetworkError(LOCATION + "Error receiving frame");
    }
    if (static_cast<uint32_t>(bytes_received) != header.size) {
        throw network_io::NetworkError(
            fmt::format("{} Expected {} bytes but received {}", LOCATION, header.size, bytes_received));
    }
    return {header, std::move(frame)};
}

/**
 * @brief receive multiple ZmqFrames (header and data)
 * @return std::vector<ZmqFrame>
 */
std::vector<ZmqFrame> ZmqSocketReceiver::receive_n() {
    std::vector<ZmqFrame> frames;
    while (true) {
        // receive header and frame
        ZmqFrame const zmq_frame = receive_zmqframe();
        if (!zmq_frame.header.data) {
            break;
        }
        frames.push_back(zmq_frame);
    }
    return frames;
}

} // namespace aare
