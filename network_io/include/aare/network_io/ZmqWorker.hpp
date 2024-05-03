#include "aare/network_io/ZmqSocketReceiver.hpp"
#include "aare/network_io/ZmqSocketSender.hpp"

namespace aare {

class ZmqWorker {
  public:
    explicit ZmqWorker(const std::string &ventilator_endpoint, const std::string &sink_endpoint = "");
    Task *pull();
    size_t push(const Task *task);
    ~ZmqWorker();

  private:
    ZmqSocketReceiver *m_receiver;
    ZmqSocketSender *m_sender;
};

} // namespace aare