#pragma once

#include "aare/network_io/ZmqSingleReceiver.hpp"

namespace aare {

class ZmqSink {
  public:
    explicit ZmqSink(const std::string &sink_endpoint);
    Task *pull();
    ~ZmqSink();

  private:
    ZmqSingleReceiver *m_receiver;
};

} // namespace aare
