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

string setup(int argc, char **argv) {
    logger::set_verbosity(logger::DEBUG);
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
        exit(1);
    }
    if (vm.count("help")) {
        cout << desc << "\n";
        exit(1);
    }

    auto port = vm["port"].as<uint16_t>();

    return "tcp://127.0.0.1:" + to_string(port);
}

int process(const std::string &endpoint) {
    // 0. connect to slsReceiver
    ZmqSocketReceiver receiver(endpoint, ZMQ_SUB);
    receiver.connect();

    // 1. create ventilator
    ZmqVentilator ventilator("tcp://*:4321");

    while (true) {
        // 2. receive frame from slsReceiver
        ZmqFrame zframe = receiver.receive_zmqframe();
        if (zframe.header.data == 0)
            continue;
        logger::info("Received frame, frame_number=", zframe.header.frameNumber);
        logger::info(zframe.header.to_string());

        // 3. create task
        Task *task = Task::init(zframe.frame.data(), zframe.frame.size());
        task->opcode = (size_t)Task::Operation::PEDESTAL;
        task->id = zframe.header.frameNumber;

        // 4. push task to ventilator
        ventilator.push(task);
        Task::destroy(task);
    }
}
int main(int argc, char **argv) {
    // read the command line arguments
    string endpoint = setup(argc, argv);
    int ret = process(endpoint);
    return ret;
}