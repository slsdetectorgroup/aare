#include "aare/network_io/ZmqSingleReceiver.hpp"
#include "aare/utils/logger.hpp"

#include <fmt/core.h>
#include <zmq.h>

namespace aare {

/**
 * @brief Construct a new ZmqSingleReceiver object
 */
ZmqSingleReceiver::ZmqSingleReceiver(const std::string &endpoint) {
    m_endpoint = endpoint;
    memset(m_header_buffer, 0, m_max_header_size);
}

/**
 * @brief Connect to the given endpoint
 * subscribe to a Zmq published
 */
void ZmqSingleReceiver::connect() {
    m_context = zmq_ctx_new();
    m_socket = zmq_socket(m_context, ZMQ_SUB);
    fmt::print("Setting ZMQ_RCVHWM to {}\n", m_zmq_hwm);
    int rc = zmq_setsockopt(m_socket, ZMQ_RCVHWM, &m_zmq_hwm, sizeof(m_zmq_hwm)); // should be set before connect
    if (rc)
        throw network_io::NetworkError(fmt::format("Could not set ZMQ_RCVHWM: {}", zmq_strerror(errno)));

    size_t bufsize = m_potential_frame_size * m_zmq_hwm;
    fmt::print("Setting ZMQ_RCVBUF to: {} MB\n", bufsize / (static_cast<size_t>(1024) * 1024));
    rc = zmq_setsockopt(m_socket, ZMQ_RCVBUF, &bufsize, sizeof(bufsize));
    if (rc)
        throw network_io::NetworkError(fmt::format("Could not set ZMQ_RCVBUF: {}", zmq_strerror(errno)));

    zmq_connect(m_socket, m_endpoint.c_str());
    zmq_setsockopt(m_socket, ZMQ_SUBSCRIBE, "", 0);
}

/**
 * @brief receive a ZmqHeader
 * @return ZmqHeader
 */
ZmqHeader ZmqSingleReceiver::receive_header() {

    // receive string ZmqHeader
    aare::logger::debug("Receiving header");
    int const header_bytes_received = zmq_recv(m_socket, m_header_buffer, m_max_header_size, 0);
    aare::logger::debug("Bytes: ", header_bytes_received);

    m_header_buffer[header_bytes_received] = '\0'; // make sure we zero terminate
    if (header_bytes_received < 0) {
        throw network_io::NetworkError(LOCATION + "Error receiving header");
    }
    aare::logger::debug("Bytes: ", header_bytes_received, ", Header: ", m_header_buffer);

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
int ZmqSingleReceiver::receive_data(std::byte *data, size_t size) {
    int const data_bytes_received = zmq_recv(m_socket, data, size, 0);
    if (data_bytes_received == -1)
        throw network_io::NetworkError("Got half of a multipart msg!!!");
    aare::logger::debug("Bytes: ", data_bytes_received);

    return data_bytes_received;
}

/**
 * @brief receive a ZmqFrame (header and data)
 * @return ZmqFrame
 */
ZmqFrame ZmqSingleReceiver::receive_zmqframe() {
    // receive header from zmq and parse it
    ZmqHeader header = receive_header();

    if (!header.data) {
        // no data following header
        return {header, Frame(0, 0, 0)};
    }

    // receive frame data
    Frame frame(header.npixelsx, header.npixelsy, header.dynamicRange);
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
std::vector<ZmqFrame> ZmqSingleReceiver::receive_n() {
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
