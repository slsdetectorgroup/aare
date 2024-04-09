#pragma once
#include "aare/Frame.hpp"
#include "aare/network_io/ZmqHeader.hpp"
#include "aare/network_io/ZmqSocket.hpp"
#include "aare/network_io/defs.hpp"

namespace aare {
class ZmqSocketSender : public ZmqSocket {
  public:
    ZmqSocketSender(const std::string &endpoint);
    void bind();
    size_t send(const ZmqHeader &header, const std::byte *data, size_t size);
    size_t send(const ZmqFrame &zmq_frame);
    size_t send(const std::vector<ZmqFrame> &zmq_frames);
};
} // namespace aare