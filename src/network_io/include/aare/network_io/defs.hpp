#pragma once
#include "aare/core/Frame.hpp"
#include "aare/network_io/ZmqHeader.hpp"

#include <stdexcept>
#include <string>

namespace aare {

/**
 * @brief ZmqFrame structure
 * wrapper class to contain a ZmqHeader and a Frame
 */
struct ZmqFrame {
    ZmqFrame(const ZmqHeader& header_, const Frame& frame_) : header(header_), frame(frame_) {}

    const ZmqHeader& header;
    const Frame& frame;
    std::string to_string() const {
        return "ZmqFrame{header: " + header.to_string() + ", frame:\nrows: " + std::to_string(frame.rows()) +
               ", cols: " + std::to_string(frame.cols()) + ", bitdepth: " + std::to_string(frame.bitdepth()) + "\n}";
    }
    size_t size() const { return frame.size() + header.size; }

};

struct Task {

    size_t id{};
    int opcode{}; // operation to perform on the data (what type should this be? char*? enum?)
    size_t data_size{};
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    std::byte payload[]; // NOLINT
#pragma GCC diagnostic pop

    static const size_t MAX_DATA_SIZE = 1024 * 1024; // 1MB
    size_t size() const { return sizeof(Task) + data_size; }
    static Task *init(std::byte *data, size_t data_size) {
        Task *task = reinterpret_cast<Task *>(new std::byte[sizeof(Task) + data_size]);
        task->data_size = data_size;
        if (data_size > 0)
            memcpy(task->payload, data, data_size);
        return task;
    }
    static int destroy(Task *task) {
        delete[] task;
        return 0;
    }

    Task() = delete;
    Task(Task &) = delete;
    Task(Task &&) = default;

    // common operations to perform
    // users can still send custom operations
    enum class Operation {
        PEDESTAL,
        PEDESTAL_AND_SAVE,
        PEDESTAL_AND_CLUSTER,
        PEDESTAL_AND_CLUSTER_AND_SAVE,
        COUNT,
    };
#ifdef AARE_MSVC
} __declspec(align(1));
// msvc does not support packed attribute
// TODO: check if this is the correct way to do this in msvc
// maybe use #pragma pack(push, 1) and #pragma pack(pop)

#else
} __attribute__((packed));
#endif
namespace network_io {
/**
 * @brief NetworkError exception class
 */
class NetworkError : public std::runtime_error {
  private:
    const char *m_msg;

  public:
    explicit NetworkError(const char *msg) : std::runtime_error(msg), m_msg(msg) {}
    explicit NetworkError(const std::string &msg) : std::runtime_error(msg), m_msg(strdup(msg.c_str())) {}
    const char *what() const noexcept override { return m_msg; }
};
} // namespace network_io

} // namespace aare