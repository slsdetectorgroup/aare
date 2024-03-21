#include <catch2/catch_all.hpp>
#include "aare/ProducerConsumerQueue.hpp"

// using arve::SimpleQueue;
TEST_CASE("push pop"){

    folly::ProducerConsumerQueue<int> q(5); 
    int a = 3;
    int b = 8;
    CHECK(q.sizeGuess() == 0);
    CHECK(q.write(a));
    CHECK(q.sizeGuess() == 1);
    CHECK(q.write(b));
    CHECK(q.sizeGuess() == 2);
    int c = 0;

    CHECK(q.read(c));
    CHECK(c == 3);
    CHECK(q.sizeGuess() == 1);
    CHECK(q.read(c));
    CHECK(c == 8);
    CHECK(q.sizeGuess() == 0);
}

TEST_CASE("Cannot push to a full queue"){
    folly::ProducerConsumerQueue<int> q(3); 
    int a = 3;
    int b = 4;
    int c = 0;
    CHECK(q.write(a));
    CHECK(q.write(b));
    CHECK_FALSE(q.write(a));

    //values are still ok
    CHECK(q.read(c));
    CHECK(c == 3);
    CHECK(q.read(c));
    CHECK(c == 4);
}

TEST_CASE("Cannot pop from an empty queue"){
    folly::ProducerConsumerQueue<int> q(2);
    int a=0; 
    CHECK_FALSE(q.read(a));
}

// TEST_CASE("fail"){
//     CHECK(false);
// }