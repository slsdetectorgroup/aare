#include "aare/core/Frame.hpp"
#include "aare/core/Transforms.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace aare;

TEST_CASE("test identity transformation") {
    Frame f(10, 10, Dtype::INT32);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            f.set<int32_t>(i, j, i + j);
        }
    }
    Frame &f2 = Transforms::identity()(f);
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
    Frame &f2 = Transforms::zero()(f);
    for (int j = 0; j < 10; j++) {
        REQUIRE((f.get_t<double>(0, j) == 0));
    }
    for (int i = 0; i < 10; i++) {
        REQUIRE((f2.get_t<double>(0, i) == 0));
    }
}

TEST_CASE("test flip_horizental transformation") {
    Frame f(10, 10, Dtype::INT32);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            f.set<int32_t>(i, j, i * 10 + j);
        }
    }
    Frame &f2 = Transforms::flip_horizental()(f);
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

TEST_CASE("test horizontal flip odd rows") {
    Frame f(5, 10, Dtype::INT32);
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 10; j++) {
            f.set<int32_t>(i, j, i * 10 + j);
        }
    }
    Frame &f2 = Transforms::flip_horizental()(f);
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 10; j++) {
            REQUIRE((f2.get_t<int32_t>(i, j) == (4 - i) * 10 + j));
        }
    }
}

TEST_CASE("test that two horizontal flips are identity") {
    Frame f(79, 9, Dtype::INT32);
    for (int i = 0; i < 79; i++) {
        for (int j = 0; j < 9; j++) {
            f.set<int32_t>(i, j, i * 79 + j);
        }
    }
    Frame &f2 = Transforms::flip_horizental()(f);
    Frame &f3 = Transforms::flip_horizental()(f2);
    for (int i = 0; i < 79; i++) {
        for (int j = 0; j < 9; j++) {
            REQUIRE(f3.get_t<int32_t>(i, j) == i * 79 + j);
        }
    }
}

TEST_CASE("test reorder") {
    SECTION("test with int32") {
        Frame f(10, 10, Dtype::INT32);
        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 10; j++) {
                f.set<int32_t>(i, j, i * 10 + j);
            }
        }
        SECTION("change only two elements in the order map") {

            Frame tmp = f.copy();
            NDArray<size_t, 2> order_map({10, 10});
            for (int i = 0; i < 10; i++) {
                for (int j = 0; j < 10; j++) {
                    order_map(i, j) = i * 10 + j;
                }
            }
            order_map(0, 0) = 99;
            order_map(9, 9) = 0;
            Frame &f2 = Transforms::reorder(order_map)(tmp);
            for (int i = 0; i < 10; i++) {
                for (int j = 0; j < 10; j++) {
                    if (i == 0 && j == 0) {
                        REQUIRE(f2.get_t<int32_t>(i, j) == 99);
                    } else if (i == 9 && j == 9) {
                        REQUIRE(f2.get_t<int32_t>(i, j) == 0);
                    } else {
                        REQUIRE(f2.get_t<int32_t>(i, j) == i * 10 + j);
                    }
                }
            }
        }

        SECTION("test flip_horizental and reorder") {
            Frame tmp = f.copy();
            // flip frame horizontally
            NDArray<size_t, 2> order_map({10, 10});
            for (int i = 0; i < 10; i++) {
                for (int j = 0; j < 10; j++) {
                    order_map(i, j) = (9 - i) * 10 + j;
                }
            }
            Frame &f2 = Transforms::reorder(order_map)(tmp);
            Frame &f3 = Transforms::flip_horizental()(f2);
            for (int i = 0; i < 10; i++) {
                for (int j = 0; j < 10; j++) {
                    REQUIRE(f3.get_t<int32_t>(i, j) == f.get_t<int32_t>(i, j));
                }
            }
        }
    }
    SECTION("test with double") {
        Frame f(10, 10, Dtype::DOUBLE);
        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 10; j++) {
                f.set<double>(i, j, i * 10 + j);
            }
        }
        // flip vertically
        Frame tmp = f.copy();
        NDArray<size_t, 2> order_map({10, 10});
        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 10; j++) {
                order_map(i, j) = i * 10 + (9 - j);
            }
        }
        Frame &f2 = Transforms::reorder(order_map)(tmp);
        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 10; j++) {
                REQUIRE(f2.get_t<double>(i, j) == f.get_t<double>(i, 9 - j));
            }
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
    Transforms transforms({
        Transforms::zero(),
    });

    Frame &f2 = transforms(f);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            REQUIRE((f2.get_t<int32_t>(i, j) == 0));
        }
    }
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            REQUIRE(f.get_t<int32_t>(i, j) == 0);
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
    Transforms transforms({
        Transforms::identity(),
        Transforms::flip_horizental(),
        Transforms::identity(),
    });
    Frame &f2 = transforms(f);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            REQUIRE((f2.get_t<int32_t>(i, j) == (9 - i) * 10 + j));
        }
    }
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            REQUIRE(f.get_t<int32_t>(i, j) == f2.get_t<int32_t>(i, j));
        }
    }
}