#include <condition_variable>
#include <limits>
#include <mutex>
#include <stddef.h>
#include <stdexcept>
#include <thread>
#include <vector>

namespace aare {
/**
 * @brief Ring buffer of slots shared between one producer and a fixed number
 * of consumers. Item i is stored in slot i % size(). The producer can only
 * overwrite a slot once all consumers released the item stored in it, and a
 * consumer can only read item i once the producer committed it.
 * Calling abort() wakes up all waiting threads.
 */
template <typename ItemType> class RingBufferPool {
  public:
    /**
     * @param size number of slots in the pool
     * @param num_consumers number of consumers that read every item
     * @param initializer callable returning the initial ItemType of a slot
     */
    template <typename Func>
    RingBufferPool(size_t size, size_t num_consumers, Func initializer)
        : m_num_consumers(num_consumers), m_item_in_slot(size, no_item),
          m_pending_consumers(size, 0) {
        if (size == 0)
            throw std::invalid_argument(
                "RingBufferPool size must be at least 1");
        m_buffers.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            m_buffers.emplace_back(initializer());
        }
    }

    /// @brief Get the number of slots in the buffer pool.
    /// @return The number of slots in the buffer pool.
    size_t size() const { return m_buffers.size(); }

    /**
     * @brief Block until the slot of item i is free (e.g. all consumers have
     * read it) to be written.
     * @return the buffer to write item i into or nullptr if aborted
     */
    ItemType *acquire_write(size_t i) {
        const size_t slot = i % size();
        std::unique_lock<std::mutex> lock(m_mtx);
        m_cv_free.wait(
            lock, [&] { return m_abort || m_pending_consumers[slot] == 0; });
        if (m_abort)
            return nullptr;
        return &m_buffers[slot];
    }

    /**
     * @brief Mark item i as written and wake up the consumers.
     */
    void commit_write(size_t i) {
        const size_t slot = i % size();
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            m_item_in_slot[slot] = i;
            m_pending_consumers[slot] = m_num_consumers;
        }
        m_cv_loaded.notify_all();
    }

    /**
     * @brief Block until item i has been written.
     * @return the buffer holding item i or nullptr if aborted
     */
    const ItemType *acquire_read(size_t i) {
        const size_t slot = i % size();
        std::unique_lock<std::mutex> lock(m_mtx);
        m_cv_loaded.wait(lock,
                         [&] { return m_abort || m_item_in_slot[slot] == i; });
        if (m_abort)
            return nullptr;
        return &m_buffers[slot];
    }

    /**
     * @brief Signal that this consumer no longer needs item i. Once all
     * consumers released it the slot can be reused by the producer.
     */
    void release_read(size_t i) {
        const size_t slot = i % size();
        bool slot_free = false;
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            if (m_item_in_slot[slot] != i)
                throw std::logic_error(
                    "release_read called for item not in slot");
            slot_free = --m_pending_consumers[slot] == 0;
        }
        if (slot_free) {
            m_cv_free.notify_one();
        }
    }

    /**
     * @brief Wake up all waiting threads, acquire_* returns nullptr from now
     * on.
     */
    void abort() {
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            m_abort = true;
        }
        m_cv_loaded.notify_all();
        m_cv_free.notify_all();
    }

  private:
    size_t m_num_consumers;
    std::vector<ItemType> m_buffers;

    static constexpr size_t no_item = std::numeric_limits<size_t>::max();

    // per slot:
    /// @brief Index of the item currently committed to the slot.
    std::vector<size_t> m_item_in_slot; // store the item - boolean flag need to
                                        // set back to false ->
    std::vector<size_t> m_pending_consumers;

    std::mutex m_mtx;
    /// @brief Condition variable to notify consumers that an item has been
    /// loaded.
    std::condition_variable m_cv_loaded;
    /// @brief Condition variable to notify producers that a slot is free to
    /// overwrite.
    std::condition_variable m_cv_free;

    /// @brief Flag to indicate if the buffer pool should be aborted.
    bool m_abort{false};
};

} // namespace aare