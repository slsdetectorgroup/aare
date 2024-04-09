#pragma once

#include "aare/core/Frame.hpp"
#include "aare/network_io/ZmqHeader.hpp"
#include "aare/network_io/ZmqSocket.hpp"
#include "aare/network_io/defs.hpp"

#include <cstdint>
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
    std::vector<ZmqFrame> receive_n();

  private:
    int receive_data(std::byte *data, size_t size);
    ZmqFrame receive_zmqframe();
    ZmqHeader receive_header();
};

} // namespace aare