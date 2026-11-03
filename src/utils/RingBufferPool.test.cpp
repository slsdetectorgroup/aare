// SPDX-License-Identifier: MPL-2.0
#include "aare/utils/RingBufferPool.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("RingBufferPool cant overwrite before all consumers release",
          "[RingBufferPool]") {
    constexpr size_t pool_size = 8;
    constexpr size_t num_consumers = 3;

    // Create a RingBufferPool with integers initialized to 0
    aare::RingBufferPool<int> ring_buffer(pool_size, num_consumers,
                                          []() { return 0; });

    // Test acquiring and committing writes
    for (size_t i = 0; i < pool_size; ++i) {
        int *buffer = ring_buffer.acquire_write(i);
        REQUIRE(buffer != nullptr);
        *buffer = static_cast<int>(i);
        ring_buffer.commit_write(i);
    }

    // Test acquiring reads
    for (size_t i = 0; i < pool_size; ++i) {
        const int *buffer = ring_buffer.acquire_read(i);
        REQUIRE(buffer != nullptr);
        REQUIRE(*buffer == static_cast<int>(i));
        ring_buffer.release_read(i);
    }

    const size_t next_buffer = pool_size; // This will wrap around to slot 0
    const size_t slot_to_write = next_buffer % pool_size;
    // Producer cannot acquire a write slot until all consumers have released it
    std::thread producer_thread([&ring_buffer, &next_buffer]() {
        int *buffer = ring_buffer.acquire_write(next_buffer);
        REQUIRE(buffer != nullptr);
        *buffer = static_cast<int>(next_buffer);
        ring_buffer.commit_write(next_buffer);
    });

    // Remaining consumers read the items
    for (size_t i = 1; i < num_consumers; ++i) {
        const int *buffer = ring_buffer.acquire_read(slot_to_write);
        REQUIRE(buffer != nullptr);
        REQUIRE(*buffer == static_cast<int>(0));
        ring_buffer.release_read(slot_to_write);
    }

    producer_thread.join();

    // written correctly
    {
        const int *buffer = ring_buffer.acquire_read(next_buffer);
        REQUIRE(buffer != nullptr);
        REQUIRE(*buffer == static_cast<int>(next_buffer));
    }
    for (size_t i = 1; i < pool_size; ++i) {
        const int *buffer = ring_buffer.acquire_read(i);
        REQUIRE(buffer != nullptr);
        REQUIRE(*buffer == static_cast<int>(i));
        ring_buffer.release_read(i);
    }

    // Test aborting the pool
    ring_buffer.abort();
    int *buffer_after_abort = ring_buffer.acquire_write(6);
    REQUIRE(buffer_after_abort == nullptr);
}

TEST_CASE("cant read if not written", "[RingBufferPool]") {
    constexpr size_t pool_size = 2;
    constexpr size_t num_consumers = 2;

    aare::RingBufferPool<int> ring_buffer(pool_size, num_consumers,
                                          []() { return 0; });

    const int value_first_slot = 50;
    const int value_second_slot = 10;

    {
        int *buffer = ring_buffer.acquire_write(0);
        *buffer = value_first_slot;
        ring_buffer.commit_write(0);
    }

    {
        const int *buffer = ring_buffer.acquire_read(0);
        REQUIRE(buffer != nullptr);
        REQUIRE(*buffer == value_first_slot);
        ring_buffer.release_read(0);
    }
    bool could_read = false;

    std::thread consumer_thread([&ring_buffer, &could_read]() {
        // Try to acquire read for an item that hasn't been written yet
        const int *buffer = ring_buffer.acquire_read(1);
        could_read = (buffer != nullptr);
        REQUIRE(*buffer == value_second_slot);
    });

    {
        const int *buffer = ring_buffer.acquire_read(0);
        REQUIRE(buffer != nullptr);
        REQUIRE(*buffer == value_first_slot);
        ring_buffer.release_read(0);
    }

    REQUIRE(could_read == false);

    {
        int *buffer = ring_buffer.acquire_write(0);
        REQUIRE(buffer != nullptr);
        *buffer = value_first_slot;
        ring_buffer.commit_write(0);
    }
    REQUIRE(could_read == false);

    {
        int *buffer = ring_buffer.acquire_write(1);
        REQUIRE(buffer != nullptr);
        *buffer = value_second_slot;
        ring_buffer.commit_write(1);
    }
    consumer_thread.join();

    REQUIRE(could_read == true);
}