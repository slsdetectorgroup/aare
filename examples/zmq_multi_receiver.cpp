#include "aare/examples/defs.hpp"
#include "aare/network_io/ZmqMultiReceiver.hpp"
#include "aare/network_io/ZmqSingleReceiver.hpp"
#include "aare/network_io/defs.hpp"

#include <boost/program_options.hpp>
#include <cassert>
#include <fmt/core.h>
#include <string>
using namespace aare;
namespace po = boost::program_options;
using namespace std;

int main() {
    logger::set_verbosity(logger::DEBUG);

    std::string const endpoint1 = "tcp://127.0.0.1:" + std::to_string(5555);
    std::string const endpoint2 = "tcp://127.0.0.1:" + std::to_string(5556);

    ZmqMultiReceiver socket({endpoint1, endpoint2}, {1, 2});

    socket.connect();
    while (true) {
        auto frames = socket.receive_n();
        logger::info("Received", frames.size(), "frames");
    }

    return 0;
}