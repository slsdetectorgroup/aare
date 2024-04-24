#pragma once
#include "aare/network_io/defs.hpp"

#include <string>
#include <vector>

namespace aare {

class ZmqMultiReceiver {
  public:
    explicit ZmqMultiReceiver(const std::vector<std::string> &endpoints);
    int connect();
    std::vector<ZmqFrame> receive_zmqframe();

  private:
    std::vector<std::string> m_endpoints;
    std::vector<ZmqSingleReceiver &> m_receivers;
    zmq_pollitem_t *items;
};

} // namespace aare