#pragma once

#include "aare/network_io/ZmqSocketReceiver.hpp"

namespace aare {

class ZmqSink {
  public:
    explicit ZmqSink(const std::string &sink_endpoint);
    Task *pull();
    ~ZmqSink();

  private:
    ZmqSocketReceiver *m_receiver;
};

} // namespace aare
