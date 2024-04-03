#include <catch2/catch_all.hpp>

#include "aare/CircularFifo.hpp"

using aare::CircularFifo;

// Only for testing. To make sure we can avoid copy constructor
// and copy assignment

struct MoveOnlyInt {
    int value{};

    MoveOnlyInt() = default;
    MoveOnlyInt(int i) : value(i){};
    MoveOnlyInt(const MoveOnlyInt &) = delete;
    MoveOnlyInt &operator=(const MoveOnlyInt &) = delete;
    MoveOnlyInt(MoveOnlyInt &&other) { std::swap(value, other.value); }
    MoveOnlyInt &operator=(MoveOnlyInt &&other) {
        std::swap(value, other.value);
        return *this;
    }
    bool operator==(int other) const { return value == other; }
};

TEST_CASE("CircularFifo can be default constructed") { CircularFifo<MoveOnlyInt> f; }

TEST_CASE("Newly constructed fifo has the right size") {
    size_t size = 17;
    CircularFifo<MoveOnlyInt> f(size);
    CHECK(f.numFreeSlots() == size);
    CHECK(f.numFilledSlots() == 0);
}

TEST_CASE("Can fit size number of objects") {
    size_t size = 8;
    size_t numPushedItems = 0;
    CircularFifo<MoveOnlyInt> f(size);
    for (size_t i = 0; i < size; ++i) {
        MoveOnlyInt a;
        bool popped = f.try_pop_free(a);
        CHECK(popped);
        if (popped) {
            a.value = i;
            bool pushed = f.try_push_value(std::move(a));
            CHECK(pushed);
            if (pushed)
                numPushedItems++;
        }
    }
    CHECK(f.numFreeSlots() == 0);
    CHECK(f.numFilledSlots() == size);
    CHECK(numPushedItems == size);
}

TEST_CASE("Push move only type") {
    CircularFifo<MoveOnlyInt> f;
    f.push_value(5);
}

TEST_CASE("Push pop") {
    CircularFifo<MoveOnlyInt> f;
    f.push_value(MoveOnlyInt(1));

    auto a = f.pop_value();
    CHECK(a == 1);
}

TEST_CASE("Pop free and then push") {
    CircularFifo<MoveOnlyInt> f;

    auto a = f.pop_free();
    a.value = 5;
    f.push_value(std::move(a)); // Explicit move since we can't copy
    auto b = f.pop_value();

    CHECK(a == 0); // Moved from value
    CHECK(b == 5); // Original value
}

TEST_CASE("Skip the first value") {
    CircularFifo<MoveOnlyInt> f;

    for (int i = 0; i != 10; ++i) {
        auto a = f.pop_free();
        a.value = i + 1;
        f.push_value(std::move(a)); // Explicit move since we can't copy
    }

    auto b = f.pop_value();
    CHECK(b == 1);
    f.next();
    auto c = f.pop_value();
    CHECK(c == 3);
}

TEST_CASE("Use in place and move to free") {
    size_t size = 18;
    CircularFifo<MoveOnlyInt> f(size);

    // Push 10 values to the fifo
    for (int i = 0; i != 10; ++i) {
        auto a = f.pop_free();
        a.value = i + 1;
        f.push_value(std::move(a)); // Explicit move since we can't copy
    }

    auto b = f.frontPtr();
    CHECK(*b == 1);
    CHECK(f.numFilledSlots() == 10);
    CHECK(f.numFreeSlots() == size - 10);
    f.next();
    auto c = f.frontPtr();
    CHECK(*c == 2);
    CHECK(f.numFilledSlots() == 9);
    CHECK(f.numFreeSlots() == size - 9);
}
