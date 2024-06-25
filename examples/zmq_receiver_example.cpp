#include "aare.hpp"

#include <cassert>

#include <fmt/core.h>
#include <string>
using namespace aare;
using namespace std;

int main(int argc, char **argv) {
    aare::logger::set_verbosity(aare::logger::DEBUG);

    ArgParser parser("Zmq receiver example");
    parser.add_option("port", "p", true, false, "5555", "port number");
    auto args = parser.parse(argc, argv);
    int port = std::stoi(args["port"]);

    std::string const endpoint = "tcp://127.0.0.1:" + std::to_string(port);
    aare::ZmqSocketReceiver socket(endpoint);
    socket.connect();
    while (true) {
        std::vector<ZmqFrame> v = socket.receive_n();
        aare::logger::info("Received ", v.size(), " frames");
        aare::logger::info("acquisition:", v[0].header.acqIndex);
        aare::logger::info("Header size:", v[0].header.to_string().size());
        aare::logger::info("Frame size:", v[0].frame.size());
        aare::logger::info("Header:", v[0].header.to_string());

        // for (ZmqFrame zmq_frame : v) {
        //     auto &[header, frame] = zmq_frame;
        //     for (int i = 0; i < 1024; i++) {
        //         for (int j = 0; j < 1024; j++) {
        //             assert(*(uint32_t *)frame.get(i, j) == (uint32_t)i + j);
        //         }
        //     }
        //     aare::logger::info("Frame verified");
        // }
    }
    return 0;
}