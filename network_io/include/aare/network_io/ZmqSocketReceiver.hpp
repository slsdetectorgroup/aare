#pragma once

#include "aare/core/Frame.hpp"
#include "aare/network_io/ZmqHeader.hpp"
#include "aare/network_io/ZmqSocket.hpp"
#include "aare/network_io/defs.hpp"

#include <cstdint>
#include <string>

// forward declare zmq_msg_t to avoid including zmq.h in the header
class zmq_msg_t;

namespace aare {

/**
 * @brief Socket to receive data from a ZMQ publisher
 * @note needs to be in sync with the main library (or maybe better use the versioning in the header)
 */
class ZmqSocketReceiver : public ZmqSocket {
  public:
    explicit ZmqSocketReceiver(const std::string &endpoint, int socket_type = 2 /* ZMQ_SUB */);
    void connect();
    void bind();
    std::vector<ZmqFrame> receive_n();

    ZmqFrame receive_zmqframe();
    ZmqHeader receive_header();
    int receive_data(std::byte *data, size_t size);
};

} // namespace aare