#include "aare/examples/defs.hpp"
#include "aare/network_io/ZmqSocketReceiver.hpp"
#include "aare/network_io/defs.hpp"

#include <boost/program_options.hpp>
#include <cassert>
#include <fmt/core.h>
#include <string>
using namespace aare;
namespace po = boost::program_options;
using namespace std;

int main(int argc, char **argv) {
    aare::logger::set_verbosity(aare::logger::DEBUG);

    po::options_description desc("options");
    desc.add_options()("help", "produce help message")("port,p", po::value<uint16_t>()->default_value(5555),
                                                       "port number");
    po::positional_options_description pd;
    pd.add("port", 1);
    po::variables_map vm;
    try {
        auto parsed = po::command_line_parser(argc, argv).options(desc).positional(pd).run();
        po::store(parsed, vm);
        po::notify(vm);

    } catch (const boost::program_options::error &e) {
        cout << e.what() << "\n";
        cout << desc << "\n";
        return 1;
    }
    if (vm.count("help")) {
        cout << desc << "\n";
        return 1;
    }

    auto port = vm["port"].as<uint16_t>();

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