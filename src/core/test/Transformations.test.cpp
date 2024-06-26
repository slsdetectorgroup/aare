#include "aare/core/Frame.hpp"
#include "aare/core/FrameTransformation.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace aare;

TEST_CASE("test identity transformation") {
    Frame f(10, 10, Dtype::INT32);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            f.set<int32_t>(i, j, i + j);
        }
    }
    Frame f2 = FrameTransformation::identity(f);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            REQUIRE((f2.get_t<int32_t>(i, j) == i + j));
        }
    }
}

TEST_CASE("test zero transformation") {
    Frame f(1, 10, Dtype::DOUBLE);
    for (int j = 0; j < 10; j++) {
        f.set<double>(0, j, j);
    }
    SECTION("test zero transformation in place") {
        auto f2 = FrameTransformation::zero(f, true);
        for (int j = 0; j < 10; j++) {
            REQUIRE((f.get_t<double>(0, j) == 0));
        }
        for (int i = 0; i < 10; i++) {
            REQUIRE((f2.get_t<double>(0, i) == 0));
        }
    }
    SECTION("test zero transformation in_place=false") {
        auto f2 = FrameTransformation::zero(f, false);
        for (int j = 0; j < 10; j++) {
            REQUIRE((f.get_t<double>(0, j) == j));
        }
        for (int i = 0; i < 10; i++) {
            REQUIRE((f2.get_t<double>(0, i) == 0));
        }
    }
}

TEST_CASE("test flip_horizental transformation") {
    Frame f(10, 10, Dtype::INT32);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            f.set<int32_t>(i, j, i * 10 + j);
        }
    }
    SECTION("test flip_horizental transformation in place") {
        auto f2 = FrameTransformation::flip_horizental(f, true);
        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 10; j++) {
                REQUIRE((f.get_t<int32_t>(i, j) == (9 - i) * 10 + j));
            }
        }
        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 10; j++) {
                REQUIRE((f2.get_t<int32_t>(i, j) == (9 - i) * 10 + j));
            }
        }
    }
    SECTION("test flip_horizental transformation in_place=false") {
        auto f2 = FrameTransformation::flip_horizental(f, false);
        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 10; j++) {
                REQUIRE((f.get_t<int32_t>(i, j) == i * 10 + j));
            }
        }
        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 10; j++) {
                REQUIRE((f2.get_t<int32_t>(i, j) == (9 - i) * 10 + j));
            }
        }
    }
}

TEST_CASE("test horizontal flip odd rows"){
    Frame f(5, 10, Dtype::INT32);
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 10; j++) {
            f.set<int32_t>(i, j, i * 10 + j);
        }
    }
    Frame f2 = FrameTransformation::flip_horizental(f, false);
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 10; j++) {
            REQUIRE((f2.get_t<int32_t>(i, j) == (4-i) * 10 + j));
        }
    }
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 10; j++) {
            REQUIRE(f.get_t<int32_t>(i, j) == i*10+j);
        }
    }
}

TEST_CASE("test that two horizontal flips are identity"){
    Frame f(79, 9, Dtype::INT32);
    for (int i = 0; i < 79; i++) {
        for (int j = 0; j < 9; j++) {
            f.set<int32_t>(i, j, i * 79 + j);
        }
    }
    Frame f2 = FrameTransformation::flip_horizental(f, false);
    Frame f3 = FrameTransformation::flip_horizental(f2, false);
    for(int i = 0; i < 79; i++){
        for(int j = 0; j < 9; j++){
            REQUIRE(f.get_t<int32_t>(i, j) == f3.get_t<int32_t>(i, j));
        }
    }
}

TEST_CASE("test chain transformation") {
    Frame f(10, 10, Dtype::INT32);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            f.set<int32_t>(i, j, i * 10 + j);
        }
    }
    Frame f2 = FrameTransformation::Chain({FrameTransformation::ZERO}).apply(f, false);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            REQUIRE((f2.get_t<int32_t>(i, j) == 0));
        }
    }
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            REQUIRE(f.get_t<int32_t>(i, j) == i*10+j);
        }
    }


}

TEST_CASE("test chain transformation with in_place") {
    Frame f(10, 10, Dtype::INT32);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            f.set<int32_t>(i, j, i * 10 + j);
        }
    }
    Frame f2 = FrameTransformation::Chain({FrameTransformation::ZERO}).apply(f, true);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            REQUIRE((f2.get_t<int32_t>(i, j) == 0));
        }
    }
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            REQUIRE(f.get_t<int32_t>(i, j) == i*10+j);
        }
    }
}

TEST_CASE("test chain transformation with multiple transformations") {
    Frame f(10, 10, Dtype::INT32);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            f.set<int32_t>(i, j, i * 10 + j);
        }
    }
    Frame f2 = FrameTransformation::Chain({FrameTransformation::IDENTITY, FrameTransformation::FLIP_HORIZENTAL,FrameTransformation::IDENTITY}).apply(f, false);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            REQUIRE((f2.get_t<int32_t>(i, j) == (9-i)*10+j));
        }
    }
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            REQUIRE(f.get_t<int32_t>(i, j) == i*10+j);
        }
    }
}