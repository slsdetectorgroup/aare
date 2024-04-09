#pragma once
#include "aare/core/Frame.hpp"
#include "aare/network_io/ZmqHeader.hpp"

#include <stdexcept>
#include <string>

namespace aare {
/**
 * @brief ZmqFrame structure
 * wrapper class to contain a ZmqHeader and a Frame
 */
struct ZmqFrame {
    ZmqHeader header;
    Frame frame;
};

namespace network_io {
/**
 * @brief NetworkError exception class
 */
class NetworkError : public std::runtime_error {
  private:
    const char *m_msg;

  public:
    NetworkError(const char *msg) : std::runtime_error(msg), m_msg(msg) {}
    NetworkError(const std::string msg) : std::runtime_error(msg) { m_msg = strdup(msg.c_str()); }
    virtual const char *what() const noexcept override { return m_msg; }
};

} // namespace network_io

} // namespace aare