#pragma once
#include "ZmqHeader.hpp"
#include "ZmqSocket.hpp"

namespace aare {
class ZmqSocketSender : public ZmqSocket {
  public:
    ZmqSocketSender(const std::string &endpoint);
    void bind();
    size_t send(ZmqHeader &header, const std::byte *data, size_t size, bool serialize_header = false);
};
} // namespace aare