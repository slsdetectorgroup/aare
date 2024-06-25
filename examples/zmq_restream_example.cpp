#include "aare.hpp"
#include "aare/examples/defs.hpp"

#include <chrono>
#include <thread>

using namespace aare;
using namespace std;

int main(int argc, char **argv) {
    aare::logger::set_verbosity(aare::logger::DEBUG);

    ArgParser parser("Zmq restream example");
    parser.add_option("file", "f", true, true, "", "input file");
    parser.add_option("port", "p", true, true, "", "port number");
    parser.add_option("fps", "fp", true, false, "1", "frames per second");
    parser.add_option("loop", "l", false, false, "", "loop over the file");

    auto args = parser.parse(argc, argv);
    std::string const path = args["file"];
    uint16_t const port = std::stoi(args["port"]);
    bool const loop = args["loop"] == "1";
    uint16_t const fps = std::stoi(args["fps"]);

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
        header.shape.row = frame.rows();
        header.shape.col = frame.cols();
        header.bitmode = frame.bitdepth();
        header.size = frame.size();

        sender.send({header, frame});
        std::this_thread::sleep_for(d);
    }
}