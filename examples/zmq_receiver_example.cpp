#include "aare/ZmqSocketReceiver.hpp"
#include <cassert>
#include <fmt/core.h>
#include <string>

int main() {
    std::string endpoint = "tcp://localhost:5555";
    aare::ZmqSocketReceiver socket(endpoint);
    socket.connect();
    char *data = new char[1024 * 1024 * 10];
    aare::ZmqHeader header;

    while (true) {
        int rc = socket.receive(header, reinterpret_cast<std::byte *>(data));
        aare::logger::info("Received bytes", rc, "Received header: ", header.to_string());
        auto *data_int = reinterpret_cast<uint32_t *>(data);
        for (uint32_t i = 0; i < header.npixelsx; i++) {
            for (uint32_t j = 0; j < header.npixelsy; j++) {
                // verify that the sent data is correct
                assert(data_int[i * header.npixelsy + j] == i + j);
            }
        }
        aare::logger::info("Frame verified");
    }
    delete[] data;
    return 0;
}