#include <string>
#include <fmt/core.h>
#include "aare/ZmqSocket.hpp"



int main(){
    std::string endpoint = "tcp://localhost:5555";
    aare::ZmqSocket socket(endpoint);
    socket.connect();
    char* data = new char[1024*1024*10];
    aare::zmqHeader header;
    while(true){
        int rc = socket.receive(header, reinterpret_cast<std::byte*>(data));

    }
    delete[] data;
    return 0;
}