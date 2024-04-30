
#pragma once
#include "aare/core/Frame.hpp"
#include "aare/network_io/ZmqHeader.hpp"
#include "aare/network_io/ZmqSocket.hpp"
#include "aare/network_io/ZmqSocketSender.hpp"
#include "aare/network_io/defs.hpp"

namespace aare {

class ZmqVentilator {
  public:
    explicit ZmqVentilator(const std::string &endpoint);
    int push(const Task* task);
    ~ZmqVentilator();

  private:
    ZmqSocketSender *m_sender;
};
} // namespace aare