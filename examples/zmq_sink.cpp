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
    // 1. bind sink to endpoint
    ZmqSink sink("tcp://*:4322");

    int i = 0;
    while (true) {
        // 2. receive Task from ventilator
        Task *task = sink.pull();
        logger::info("Received", i++, "tasks");

        Task::destroy(task);
    }
    // read the command line arguments
}