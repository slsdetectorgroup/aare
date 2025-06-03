#pragma once

#include <chrono>
#include <fmt/color.h>
#include <fmt/format.h>
#include <memory>
#include <thread>

#include "aare/ProducerConsumerQueue.hpp"

namespace aare {

template <class ItemType> class CircularFifo {
    uint32_t fifo_size;
    aare::ProducerConsumerQueue<ItemType> free_slots;
    aare::ProducerConsumerQueue<ItemType> filled_slots;

  public:
    CircularFifo() : CircularFifo(100){};
    CircularFifo(uint32_t size) : fifo_size(size), free_slots(size + 1), filled_slots(size + 1) {

        // TODO! how do we deal with alignment for writing? alignas???
        // Do we give the user a chance to provide memory locations?
        // Templated allocator?
        for (size_t i = 0; i < fifo_size; ++i) {
            free_slots.write(ItemType{});
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

    ItemType pop_free() {
        ItemType v;
        while (!free_slots.read(v))
            ;
        return std::move(v);
        // return v;
    }

    bool try_pop_free(ItemType &v) { return free_slots.read(v); }

    ItemType pop_value(std::chrono::nanoseconds wait, std::atomic<bool> &stopped) {
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

    // TODO! Add function to move item from filled to free to be used
    // with the frontPtr function

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