// SPDX-License-Identifier: MPL-2.0

#include "aare/ProducerConsumerQueue.hpp"

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <string>
#include <type_traits>

using aare::ProducerConsumerQueue;

// The queue is a fixed-capacity SPSC ring with const size and buffer, as in
// the upstream folly implementation. Moving one is not thread safe and the
// previous move operations left the source pointing at a null buffer, so the
// type is deliberately neither copyable nor movable. Hold it by value or in a
// unique_ptr instead.
static_assert(!std::is_copy_constructible_v<ProducerConsumerQueue<int>>);
static_assert(!std::is_copy_assignable_v<ProducerConsumerQueue<int>>);
static_assert(!std::is_move_constructible_v<ProducerConsumerQueue<int>>);
static_assert(!std::is_move_assignable_v<ProducerConsumerQueue<int>>);

TEST_CASE("ProducerConsumerQueue round trips non trivial elements", "[queue]") {
    ProducerConsumerQueue<std::string> q(3); // capacity 2
    REQUIRE(q.capacity() == 2);
    REQUIRE(q.isEmpty());

    REQUIRE(q.write(std::string(64, 'a')));
    REQUIRE(q.write(std::string(64, 'b')));
    CHECK(q.isFull());
    CHECK_FALSE(q.write(std::string(64, 'c')));

    std::string out;
    REQUIRE(q.read(out));
    CHECK(out == std::string(64, 'a'));

    // Wrap around the ring and leave an element behind for the destructor.
    REQUIRE(q.write(std::string(64, 'd')));
    REQUIRE(q.frontPtr() != nullptr);
    CHECK(*q.frontPtr() == std::string(64, 'b'));
    q.popFront();
    CHECK(q.sizeGuess() == 1);
}

TEST_CASE("ProducerConsumerQueue rejects sizes below two in every build type",
          "[queue]") {
    // A plain assert would be compiled out in Release builds and a zero-size
    // queue would then overflow its buffer on the first write.
    CHECK_THROWS_AS(ProducerConsumerQueue<int>(0), std::invalid_argument);
    CHECK_THROWS_AS(ProducerConsumerQueue<int>(1), std::invalid_argument);

    ProducerConsumerQueue<int> smallest(2);
    CHECK(smallest.capacity() == 1);
    CHECK(smallest.write(7));
    CHECK(smallest.isFull());
}
