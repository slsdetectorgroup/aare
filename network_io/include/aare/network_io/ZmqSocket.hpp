#pragma once

#include <string>

// Socket to receive data from a ZMQ publisher
// needs to be in sync with the main library (or maybe better use the versioning in the header)

// forward declare zmq_msg_t to avoid including zmq.h in the header
class zmq_msg_t;

namespace aare {

/**
 * @brief parent class for ZmqSocketReceiver and ZmqSocketSender
 * contains common functions and variables
 */
class ZmqSocket {
  public:
    ZmqSocket() = default;
    ~ZmqSocket();
    ZmqSocket(const ZmqSocket &) = delete;
    ZmqSocket operator=(const ZmqSocket &) = delete;
    ZmqSocket(ZmqSocket &&) = delete;
    void disconnect();
    void set_zmq_hwm(int hwm);
    void set_timeout_ms(int n);
    void set_potential_frame_size(size_t size);
    void *get_socket();

  protected:
    void *m_context{nullptr};
    void *m_socket{nullptr};
    int m_socket_type{};
    std::string m_endpoint;
    int m_zmq_hwm{1000};
    int m_timeout_ms{1000};
    size_t m_potential_frame_size{static_cast<size_t>(1024) * 1024};
    constexpr static size_t m_max_header_size = 1024;
    char *m_header_buffer = new char[m_max_header_size];
};

} // namespace aare