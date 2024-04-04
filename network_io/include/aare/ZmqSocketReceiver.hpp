#pragma once

#include "ZmqHeader.hpp"
#include "ZmqSocket.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <string>

// Socket to receive data from a ZMQ publisher
// needs to be in sync with the main library (or maybe better use the versioning in the header)

// forward declare zmq_msg_t to avoid including zmq.h in the header
class zmq_msg_t;

namespace aare {

class ZmqSocketReceiver : public ZmqSocket {
  public:
    ZmqSocketReceiver(const std::string &endpoint);
    void connect();
    int receive(ZmqHeader &header, std::byte *data, bool serialized_header = false);
};

} // namespace aare