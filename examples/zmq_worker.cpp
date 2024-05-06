
#include "aare.hpp"
#include "aare/examples/defs.hpp"

#include "zmq.h"
#include <boost/program_options.hpp>
#include <cassert>
#include <fmt/core.h>
#include <string>
using namespace aare;
namespace po = boost::program_options;
using namespace std;

int main() {
    logger::set_verbosity(logger::DEBUG);
    // 1. connect to ventilator and sink
    ZmqWorker worker("tcp://127.0.0.1:4321", "tcp://127.0.0.1:4322");

    while (true) {
        // 2. receive Task from ventilator
        Task *ventilator_task = worker.pull();
        logger::info("Received Task, id=", ventilator_task->id, " data_size=", ventilator_task->data_size);

        Task *sink_task = Task::init(nullptr, 0);
        sink_task->id = ventilator_task->id;
        sink_task->opcode = (size_t)Task::Operation::COUNT;
        worker.push(sink_task);

        Task::destroy(sink_task);
        Task::destroy(ventilator_task);
    }
    // read the command line arguments
}