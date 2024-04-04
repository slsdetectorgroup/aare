#include "aare/Frame.hpp"
#include "aare/ZmqSocketSender.hpp"
#include "aare/utils/logger.hpp"

#include <fmt/core.h>
#include <string>
#include <unistd.h> // sleep
using namespace aare;

int main() {
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
    header.imageSize = sizeof(uint32_t) * 1024 * 1024;
    header.dynamicRange = 32;

    int i = 0;
    while (true) {
        aare::logger::info("Sending frame:", i++);
        aare::logger::info("Header size:", sizeof(header.to_string()));
        aare::logger::info("Frame size:", frame.size(), "\n");

        int rc = socket.send(header, frame.data(), frame.size());
        sleep(1);
    }
    return 0;
}