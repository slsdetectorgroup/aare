#include "aare/core/Frame.hpp"
#include "aare/network_io/ZmqHeader.hpp"
#include "aare/network_io/ZmqSocketSender.hpp"
#include "aare/network_io/defs.hpp"
#include "aare/utils/logger.hpp"

#include <ctime> // std::time
#include <fmt/core.h>
#include <string>
#include <unistd.h> // sleep
using namespace aare;

int main() {
    std::srand(std::time(nullptr));
    std::string endpoint = "tcp://*:5555";
    aare::ZmqSocketSender socket(endpoint);
    socket.bind();
    Frame frame(1024, 1024, sizeof(uint32_t) * 8);
    for (int i = 0; i < 1024; i++) {
        for (int j = 0; j < 1024; j++) {
            frame.set(i, j, i + j);
        }
    }
    aare::ZmqHeader header;
    header.npixelsx = 1024;
    header.npixelsy = 1024;
    header.size = sizeof(uint32_t) * 1024 * 1024;
    header.dynamicRange = 32;

    std::vector<ZmqFrame> zmq_frames;
    // send two exact frames

    int acqid = 0;
    while (true) {
        zmq_frames.clear();
        header.acqIndex = acqid++;
        size_t n_frames = std::rand() % 10 + 1;

        aare::logger::info("acquisition:", header.acqIndex);
        aare::logger::info("Header size:", header.to_string().size());
        aare::logger::info("Frame size:", frame.size());
        aare::logger::info("Number of frames:", n_frames);

        for (size_t i = 0; i < n_frames; i++) {
            zmq_frames.push_back({header, frame});
        }
        size_t rc = socket.send(zmq_frames);
        aare::logger::info("Sent bytes", rc);
        sleep(1);
    }
    return 0;
}