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
    std::string to_string() const {
        return "ZmqFrame{header: " + header.to_string() + ", frame:\nrows: " + std::to_string(frame.rows()) + ", cols: " +
               std::to_string(frame.cols()) + ", bitdepth: " + std::to_string(frame.bitdepth()) + "\n}";
    }
};

namespace network_io {
/**
 * @brief NetworkError exception class
 */
class NetworkError : public std::runtime_error {
  private:
    const char *m_msg;

  public:
    explicit NetworkError(const char *msg) : std::runtime_error(msg), m_msg(msg) {}
    explicit NetworkError(const std::string &msg) : std::runtime_error(msg), m_msg(strdup(msg.c_str())) {}
    const char *what() const noexcept override { return m_msg; }
};

} // namespace network_io

} // namespace aare