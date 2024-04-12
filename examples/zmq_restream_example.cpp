#include <chrono>
#include <thread>

#include "aare/file_io/File.hpp"
#include "aare/network_io/ZmqSocketSender.hpp"

#include <boost/program_options.hpp>

using namespace aare;
using namespace std;
namespace po = boost::program_options;

int main(int argc, char **argv) {
    aare::logger::set_verbosity(aare::logger::DEBUG);

    po::options_description desc("Allowed options");
    desc.add_options()("help", "produce help message")("file,f", po::value<string>(), "input file")(
        "port,p", po::value<uint16_t>(), "port number")("fps", po::value<uint16_t>()->default_value(1),
                                                        "frames per second (default 1)")("loop,l",
                                                                                         "loop over the file");
    po::positional_options_description pd;
    pd.add("file", -1);

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
    if (vm.count("file") != 1) {
        aare::logger::error("file is required");
        cout << desc << "\n";
        return 1;
    }
    if (vm.count("port") != 1) {
        aare::logger::error("file is required");
        cout << desc << "\n";
        return 1;
    }

    std::string const path = vm["file"].as<string>();
    uint16_t const port = vm["port"].as<uint16_t>();
    bool const loop = vm.count("loop") == 1;
    uint16_t const fps = vm["fps"].as<uint16_t>();

    aare::logger::debug("ARGS: file:", path, "port:", port, "fps:", fps, "loop:", loop);
    auto d = round<std::chrono::milliseconds>(std::chrono::duration<double>{1. / fps});
    aare::logger::debug("sleeping for", d.count(), "ms");

    if (!std::filesystem::exists(path)) {
        aare::logger::error("file does not exist");
        return 1;
    }

    std::filesystem::path const tmp(path);

    File file(tmp, "r");
    string const endpoint = "tcp://*:" + std::to_string(port);
    ZmqSocketSender sender(endpoint);
    sender.bind();
    std::this_thread::sleep_for(d); // slow joiner problem should fix this

    for (size_t frameidx = 0; frameidx < file.total_frames(); frameidx++) {

        Frame const frame = file.read();
        ZmqHeader header;
        header.frameNumber = frameidx;
        header.data = true;
        header.npixelsx = frame.rows();
        header.npixelsy = frame.cols();
        header.dynamicRange = frame.bitdepth();
        header.size = frame.size();

        sender.send({header, frame});
        std::this_thread::sleep_for(d);
    }
}