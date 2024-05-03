#pragma once
#include "aare/network_io/ZmqSingleReceiver.hpp"
#include "aare/network_io/defs.hpp"
#include <unordered_map>

#include <string>
#include <vector>
using zmq_pollitem_t = struct zmq_pollitem_t;
namespace aare {

class ZmqMultiReceiver {
  public:
    explicit ZmqMultiReceiver(const std::vector<std::string> &endpoints, const xy &geometry = {1, 1});
    int connect();
    ZmqFrame receive_zmqframe();
    std::vector<ZmqFrame> receive_n();
    ~ZmqMultiReceiver();

  private:
    ZmqFrame receive_zmqframe_(std::unordered_map<uint64_t, std::vector<ZmqFrame>> &frames_map);
    xy m_geometry;
    std::vector<std::string> m_endpoints;
    std::vector<ZmqSingleReceiver *> m_receivers;
    zmq_pollitem_t *items{};
};

} // namespace aare