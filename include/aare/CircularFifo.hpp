// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <cassert>
#include <chrono>
#include <fmt/color.h>
#include <fmt/format.h>
#include <memory>
#include <thread>
#include <utility>

#include "aare/ProducerConsumerQueue.hpp"

namespace aare {

template <class ItemType> class CircularFifo {
    uint32_t fifo_size;
    aare::ProducerConsumerQueue<ItemType> free_slots;
    aare::ProducerConsumerQueue<ItemType> filled_slots;

  public:
    CircularFifo() : CircularFifo(100) {};
    CircularFifo(uint32_t size)
        : fifo_size(size), free_slots(size + 1), filled_slots(size + 1) {

        // TODO! how do we deal with alignment for writing? alignas???
        // Do we give the user a chance to provide memory locations?
        // Templated allocator?
        for (size_t i = 0; i < fifo_size; ++i) {
            free_slots.write(ItemType{});
        }
    }

    /**
     * @brief Construct a fifo and seed the free list using a factory.
     * @param size number of items circulating in the fifo
     * @param make_item callable invoked as make_item(i) for each slot index i
     *
     * Use this instead of CircularFifo(size) when the items need to be
     * initialized, for example to hold a preallocated buffer or to carry
     * their own slot index.
     */
    template <class F>
    CircularFifo(uint32_t size, F make_item)
        : fifo_size(size), free_slots(size + 1), filled_slots(size + 1) {
        for (size_t i = 0; i < fifo_size; ++i) {
            free_slots.write(make_item(i));
        }
    }

    bool next() {
        // TODO! avoid default constructing ItemType
        ItemType it;
        if (!filled_slots.read(it))
            return false;
        if (!free_slots.write(std::move(it)))
            return false;
        return true;
    }

    ~CircularFifo() {}

    using value_type = ItemType;

    auto numFilledSlots() const noexcept { return filled_slots.sizeGuess(); }
    auto numFreeSlots() const noexcept { return free_slots.sizeGuess(); }
    auto isFull() const noexcept { return filled_slots.isFull(); }

    /**
     * @brief True if there are no filled slots waiting to be consumed.
     * @note Prefer this over numFilledSlots() == 0 since sizeGuess() may
     * under-report when called from the producing thread.
     */
    auto isEmpty() const noexcept { return filled_slots.isEmpty(); }

    ItemType pop_free() {
        ItemType v;
        while (!free_slots.read(v))
            ;
        return std::move(v);
        // return v;
    }

    bool try_pop_free(ItemType &v) { return free_slots.read(v); }

    ItemType pop_value(std::chrono::nanoseconds wait,
                       std::atomic<bool> &stopped) {
        ItemType v;
        while (!filled_slots.read(v) && !stopped) {
            std::this_thread::sleep_for(wait);
        }
        return std::move(v);
    }

    ItemType pop_value() {
        ItemType v;
        while (!filled_slots.read(v))
            ;
        return std::move(v);
    }

    ItemType *frontPtr() { return filled_slots.frontPtr(); }

    /**
     * @brief Return the front filled item to the free list. To be used
     * together with frontPtr() once the item has been consumed in place.
     * @warning The fifo must not be empty when calling this.
     *
     * The item is written to the free list before it is popped from the
     * filled list, so it can never be dropped. The write cannot fail: both
     * queues hold size + 1 slots while only size items circulate.
     */
    void recycle_front() {
        ItemType *it = filled_slots.frontPtr();
        assert(it != nullptr);
        [[maybe_unused]] const bool ok = free_slots.write(std::move(*it));
        assert(ok);
        filled_slots.popFront();
    }

    template <class... Args> void push_value(Args &&...recordArgs) {
        while (!filled_slots.write(std::forward<Args>(recordArgs)...))
            ;
    }

    template <class... Args> bool try_push_value(Args &&...recordArgs) {
        return filled_slots.write(std::forward<Args>(recordArgs)...);
    }

    template <class... Args> void push_free(Args &&...recordArgs) {
        while (!free_slots.write(std::forward<Args>(recordArgs)...))
            ;
    }

    template <class... Args> bool try_push_free(Args &&...recordArgs) {
        return free_slots.write(std::forward<Args>(recordArgs)...);
    }
};

} // namespace aare