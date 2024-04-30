#include "aare/network_io/ZmqSingleReceiver.hpp"
#include "aare/network_io/ZmqSocketSender.hpp"

namespace aare {

class ZmqWorker {
  public:
    explicit ZmqWorker(const std::string &ventilator_endpoint, const std::string &sink_endpoint="");
    Task* pull();
    int push(const Task *task);
    ~ZmqWorker();

  private:
    ZmqSingleReceiver *m_receiver;
    ZmqSocketSender *m_sender;
};


} // namespace aare