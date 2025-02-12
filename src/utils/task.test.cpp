#include "aare/utils/task.hpp"

#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/catch_test_macros.hpp>


TEST_CASE("Split a range into multiple tasks"){    

    auto tasks = aare::split_task(0, 10, 3);
    REQUIRE(tasks.size() == 3);
    REQUIRE(tasks[0].first == 0);
    REQUIRE(tasks[0].second == 3);
    REQUIRE(tasks[1].first == 3);
    REQUIRE(tasks[1].second == 6);
    REQUIRE(tasks[2].first == 6);
    REQUIRE(tasks[2].second == 10);

    tasks = aare::split_task(0, 10, 1);
    REQUIRE(tasks.size() == 1);
    REQUIRE(tasks[0].first == 0);
    REQUIRE(tasks[0].second == 10);

    tasks = aare::split_task(0, 10, 10);
    REQUIRE(tasks.size() == 10);
    for (int i = 0; i < 10; i++){
        REQUIRE(tasks[i].first == i);
        REQUIRE(tasks[i].second == i+1);
    }
    


}