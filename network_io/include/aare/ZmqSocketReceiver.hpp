#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>

// Socket to receive data from a ZMQ publisher
// needs to be in sync with the main library (or maybe better use the versioning in the header)

// forward declare zmq_msg_t to avoid including zmq.h in the header
class zmq_msg_t;

namespace aare {

/** zmq header structure (from slsDetectorPackage)*/
struct zmqHeader {
    /** true if incoming data, false if end of acquisition */
    bool data{true};
    uint32_t jsonversion{0};
    uint32_t dynamicRange{0};
    uint64_t fileIndex{0};
    /** number of detectors/port in x axis */
    uint32_t ndetx{0};
    /** number of detectors/port in y axis */
    uint32_t ndety{0};
    /** number of pixels/channels in x axis for this zmq socket */
    uint32_t npixelsx{0};
    /** number of pixels/channels in y axis for this zmq socket */
    uint32_t npixelsy{0};
    /** number of bytes for an image in this socket */
    uint32_t imageSize{0};
    /** frame number from detector */
    uint64_t acqIndex{0};
    /** frame index (starting at 0 for each acquisition) */
    uint64_t frameIndex{0};
    /** progress in percentage */
    double progress{0};
    /** file name prefix */
    std::string fname;
    /** header from detector */
    uint64_t frameNumber{0};
    uint32_t expLength{0};
    uint32_t packetNumber{0};
    uint64_t detSpec1{0};
    uint64_t timestamp{0};
    uint16_t modId{0};
    uint16_t row{0};
    uint16_t column{0};
    uint16_t detSpec2{0};
    uint32_t detSpec3{0};
    uint16_t detSpec4{0};
    uint8_t detType{0};
    uint8_t version{0};
    /** if rows of image should be flipped */
    int flipRows{0};
    /** quad type (eiger hardware specific) */
    uint32_t quad{0};
    /** true if complete image, else missing packets */
    bool completeImage{false};
    /** additional json header */
    std::map<std::string, std::string> addJsonHeader;
    /** (xmin, xmax, ymin, ymax) roi only in files written */
    std::array<int, 4> rx_roi{};
};

class ZmqSocketReceiver {
    void *m_context{nullptr};
    void *m_socket{nullptr};
    std::string m_endpoint;
    int m_zmq_hwm{1000};
    int m_timeout_ms{1000};
    constexpr static size_t m_max_header_size = 1024;
    char *m_header_buffer = new char[m_max_header_size];

    bool decode_header(zmqHeader &h);

  public:
    ZmqSocketReceiver(const std::string &endpoint);
    ~ZmqSocketReceiver();
    ZmqSocketReceiver(const ZmqSocketReceiver &) = delete;
    ZmqSocketReceiver operator=(const ZmqSocketReceiver &) = delete;
    ZmqSocketReceiver(ZmqSocketReceiver &&) = delete;

    void connect();
    void disconnect();
    void set_zmq_hwm(int hwm);
    void set_timeout_ms(int n);

    int receive(zmqHeader &header, std::byte *data);
};

} // namespace aare