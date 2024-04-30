#pragma once
#include "aare/core/Frame.hpp"
#include "aare/network_io/ZmqHeader.hpp"
#include "aare/network_io/ZmqSocket.hpp"
#include "aare/network_io/defs.hpp"

namespace aare {

/**
 * @brief Socket to send data to a ZMQ subscriber
 * @note needs to be in sync with the main library (or maybe better use the versioning in the header)
 */
class ZmqSocketSender : public ZmqSocket {
  public:
    explicit ZmqSocketSender(const std::string &endpoint, int socket_type = 1 /* ZMQ_PUB */);
    void connect();
    void bind();
    size_t send(const void* data, size_t size);
    size_t send(const ZmqHeader &header, const void *data, size_t size);
    size_t send(const ZmqFrame &zmq_frame);
    size_t send(const std::vector<ZmqFrame> &zmq_frames);
};
} // namespace aare