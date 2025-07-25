#include "aare/NDArray.hpp"
#include <array>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <numeric>

using aare::NDArray;
using aare::NDView;
using aare::Shape;

TEST_CASE("Initial size is zero if no size is specified") {
    NDArray<double> a;
    REQUIRE(a.size() == 0);
    REQUIRE(a.shape() == Shape<2>{0, 0});
}

TEST_CASE("Construct from an NDView") {
    std::vector<int> some_data(9, 42);
    NDView<int, 2> view(some_data.data(), Shape<2>{3, 3});

    NDArray<int, 2> image(view);

    REQUIRE(image.shape() == view.shape());
    REQUIRE(image.size() == view.size());
    REQUIRE(image.data() != view.data());

    for (uint32_t i = 0; i < image.size(); ++i) {
        REQUIRE(image[i] == view[i]);
    }

    // Changing the image doesn't change the view
    image = 43;
    for (uint32_t i = 0; i < image.size(); ++i) {
        REQUIRE(image[i] != view[i]);
    }
}

TEST_CASE("3D NDArray from NDView") {
    std::vector<int> data(27);
    std::iota(data.begin(), data.end(), 0);
    NDView<int, 3> view(data.data(), Shape<3>{3, 3, 3});
    NDArray<int, 3> image(view);
    REQUIRE(image.shape() == view.shape());
    REQUIRE(image.size() == view.size());
    REQUIRE(image.data() != view.data());

    for (ssize_t i = 0; i < image.shape(0); i++) {
        for (ssize_t j = 0; j < image.shape(1); j++) {
            for (ssize_t k = 0; k < image.shape(2); k++) {
                REQUIRE(image(i, j, k) == view(i, j, k));
            }
        }
    }
}

TEST_CASE("1D image") {
    std::array<ssize_t, 1> shape{{20}};
    NDArray<short, 1> img(shape, 3);
    REQUIRE(img.size() == 20);
    REQUIRE(img(5) == 3);
}

TEST_CASE("Accessing a const object") {
    const NDArray<double, 3> img({3, 4, 5}, 0);
    REQUIRE(img(1, 1, 1) == 0);
    REQUIRE(img.size() == 3 * 4 * 5);
    REQUIRE(img.shape() == Shape<3>{3, 4, 5});
    REQUIRE(img.shape(0) == 3);
    REQUIRE(img.shape(1) == 4);
    REQUIRE(img.shape(2) == 5);
}

TEST_CASE("Indexing of a 2D image") {
    std::array<ssize_t, 2> shape{{3, 7}};
    NDArray<long> img(shape, 5);
    for (uint32_t i = 0; i != img.size(); ++i) {
        REQUIRE(img(i) == 5);
    }

    for (uint32_t i = 0; i != img.size(); ++i) {
        img(i) = i;
    }
    REQUIRE(img(0, 0) == 0);
    REQUIRE(img(0, 1) == 1);
    REQUIRE(img(1, 0) == 7);
}

TEST_CASE("Indexing of a 3D image") {
    NDArray<float, 3> img{{{3, 4, 2}}, 5.0f};
    for (uint32_t i = 0; i != img.size(); ++i) {
        REQUIRE(img(i) == 5.0f);
    }

    // Double check general properties
    REQUIRE(img.size() == 3 * 4 * 2);

    for (uint32_t i = 0; i != img.size(); ++i) {
        img(i) = float(i);
    }
    REQUIRE(img(0, 0, 0) == 0);
    REQUIRE(img(0, 0, 1) == 1);
    REQUIRE(img(0, 1, 1) == 3);
    REQUIRE(img(1, 2, 0) == 12);
    REQUIRE(img(2, 3, 1) == 23);
}

TEST_CASE("Divide double by int") {
    NDArray<double, 1> a{{5}, 5};
    NDArray<int, 1> b{{5}, 5};
    a /= b;
    for (auto it : a) {
        REQUIRE(it == 1.0);
    }
}

TEST_CASE("Elementwise multiplication of 3D image") {
    std::array<ssize_t, 3> shape{3, 4, 2};
    NDArray<double, 3> a{shape};
    NDArray<double, 3> b{shape};
    for (uint32_t i = 0; i != a.size(); ++i) {
        a(i) = i;
        b(i) = i;
    }
    // auto c = a * b; // This works but the result is a lazy ArrayMul object
    NDArray<double, 3> c = a * b;
    REQUIRE(c(0, 0, 0) == 0 * 0);
    REQUIRE(c(0, 0, 1) == 1 * 1);
    REQUIRE(c(0, 1, 1) == 3 * 3);
    REQUIRE(c(1, 2, 0) == 12 * 12);
    REQUIRE(c(2, 3, 1) == 23 * 23);
}

NDArray<int> MultiplyNDArrayUsingOperator(NDArray<int> &a, NDArray<int> &b) {
    // return a * a * b * b;
    NDArray<int> c = a * b;
    return c;
}

NDArray<int> MultiplyNDArrayUsingIndex(NDArray<int> &a, NDArray<int> &b) {
    NDArray<int> res(a.shape());
    for (uint32_t i = 0; i < a.size(); i++) {
        // res(i) = a(i) * a(i) * b(i) * b(i);
        res(i) = a(i) * b(i);
    }
    return res;
}

NDArray<int> AddNDArrayUsingOperator(NDArray<int> &a, NDArray<int> &b) {
    // return a * a * b * b;
    // NDArray<int>c = a+b;
    NDArray<int> c(a.shape());
    c = a + b;
    return c;
}

NDArray<int> AddNDArrayUsingIndex(NDArray<int> &a, NDArray<int> &b) {
    NDArray<int> res(a.shape());
    for (uint32_t i = 0; i < a.size(); i++) {
        // res(i) = a(i) * a(i) * b(i) * b(i);
        res(i) = a(i) + b(i);
    }
    return res;
}

TEST_CASE("Compare two images") {
    NDArray<int> a;
    NDArray<int> b;
    CHECK((a == b));

    a = NDArray<int>{{5, 10}, 0};
    CHECK((a != b));

    b = NDArray<int>{{5, 10}, 0};
    CHECK((a == b));

    b(3, 3) = 7;
    CHECK((a != b));
}

TEST_CASE("Size and shape matches") {
    ssize_t w = 15;
    ssize_t h = 75;
    std::array<ssize_t, 2> shape{w, h};
    NDArray<double> a{shape};
    REQUIRE(a.size() == w * h);
    REQUIRE(a.shape() == shape);
}

TEST_CASE("Initial value matches for all elements") {
    double v = 4.35;
    NDArray<double> a{{5, 5}, v};
    for (int i = 0; i < a.size(); ++i) {
        REQUIRE(a(i) == v);
    }
}

TEST_CASE("Data layout of 3D image, fast index last") {
    NDArray<int, 3> a{{3, 3, 3}, 0};
    REQUIRE(a.size() == 27);
    int *ptr = a.data();

    for (int i = 0; i < 9; ++i) {
        *ptr++ = 10 + i;
        REQUIRE(a(0, 0, i) == 10 + i);
        REQUIRE(a(i) == 10 + i);
    }
}

TEST_CASE("Bitwise and on data") {

    NDArray<uint16_t, 1> a({3}, 0);
    uint16_t mask = 0x3FF;
    a(0) = 16684;
    a(1) = 33068;
    a(2) = 52608;

    a &= mask;

    REQUIRE(a(0) == 300);
    REQUIRE(a(1) == 300);
    REQUIRE(a(2) == 384);
}

TEST_CASE("Elementwise operations on images") {
    std::array<ssize_t, 2> shape{5, 5};
    double a_val = 3.0;
    double b_val = 8.0;

    SECTION("Add two images") {
        NDArray<double> A(shape, a_val);
        NDArray<double> B(shape, b_val);

        NDArray<double> C = A + B;
        // auto C = A+B; // This works but the result is a lazy ArraySum object

        // Value of C matches
        for (uint32_t i = 0; i < C.size(); ++i) {
            REQUIRE(C(i) == a_val + b_val);
        }

        // Value of A is not changed
        for (uint32_t i = 0; i < A.size(); ++i) {
            REQUIRE(A(i) == a_val);
        }

        // Value of B is not changed
        for (uint32_t i = 0; i < B.size(); ++i) {
            REQUIRE(B(i) == b_val);
        }

        // A, B and C referes to different data
        REQUIRE(A.data() != B.data());
        REQUIRE(B.data() != C.data());
    }
    SECTION("Subtract two images") {
        NDArray<double> A(shape, a_val);
        NDArray<double> B(shape, b_val);
        NDArray<double> C = A - B;
        // auto C = A - B; // This works but the result is a lazy ArraySub
        // object

        // Value of C matches
        for (uint32_t i = 0; i < C.size(); ++i) {
            REQUIRE(C(i) == a_val - b_val);
        }

        // Value of A is not changed
        for (uint32_t i = 0; i < A.size(); ++i) {
            REQUIRE(A(i) == a_val);
        }

        // Value of B is not changed
        for (uint32_t i = 0; i < B.size(); ++i) {
            REQUIRE(B(i) == b_val);
        }

        // A, B and C referes to different data
        REQUIRE(A.data() != B.data());
        REQUIRE(B.data() != C.data());
    }
    SECTION("Multiply two images") {
        NDArray<double> A(shape, a_val);
        NDArray<double> B(shape, b_val);
        // auto C = A * B; // This works but the result is a lazy ArrayMul
        // object
        NDArray<double> C = A * B;

        // Value of C matches
        for (uint32_t i = 0; i < C.size(); ++i) {
            REQUIRE(C(i) == a_val * b_val);
        }

        // Value of A is not changed
        for (uint32_t i = 0; i < A.size(); ++i) {
            REQUIRE(A(i) == a_val);
        }

        // Value of B is not changed
        for (uint32_t i = 0; i < B.size(); ++i) {
            REQUIRE(B(i) == b_val);
        }

        // A, B and C referes to different data
        REQUIRE(A.data() != B.data());
        REQUIRE(B.data() != C.data());
    }
    SECTION("Divide two images") {
        NDArray<double> A(shape, a_val);
        NDArray<double> B(shape, b_val);
        // auto C = A / B; // This works but the result is a lazy ArrayDiv
        // object
        NDArray<double> C = A / B;

        // Value of C matches
        for (uint32_t i = 0; i < C.size(); ++i) {
            REQUIRE(C(i) == a_val / b_val);
        }

        // Value of A is not changed
        for (uint32_t i = 0; i < A.size(); ++i) {
            REQUIRE(A(i) == a_val);
        }

        // Value of B is not changed
        for (uint32_t i = 0; i < B.size(); ++i) {
            REQUIRE(B(i) == b_val);
        }

        // A, B and C referes to different data
        REQUIRE(A.data() != B.data());
        REQUIRE(B.data() != C.data());
    }

    SECTION("subtract scalar") {
        NDArray<double> A(shape, a_val);
        NDArray<double> B(shape, b_val);
        double v = 1.0;
        auto C = A - v;
        REQUIRE(C.data() != A.data());

        // Value of C matches
        for (uint32_t i = 0; i < C.size(); ++i) {
            REQUIRE(C(i) == a_val - v);
        }

        // Value of A is not changed
        for (uint32_t i = 0; i < A.size(); ++i) {
            REQUIRE(A(i) == a_val);
        }
    }
    SECTION("add scalar") {
        NDArray<double> A(shape, a_val);
        NDArray<double> B(shape, b_val);
        double v = 1.0;
        auto C = A + v;
        REQUIRE(C.data() != A.data());

        // Value of C matches
        for (uint32_t i = 0; i < C.size(); ++i) {
            REQUIRE(C(i) == a_val + v);
        }

        // Value of A is not changed
        for (uint32_t i = 0; i < A.size(); ++i) {
            REQUIRE(A(i) == a_val);
        }
    }
    SECTION("divide with scalar") {
        NDArray<double> A(shape, a_val);
        NDArray<double> B(shape, b_val);
        double v = 3.7;
        auto C = A / v;
        REQUIRE(C.data() != A.data());

        // Value of C matches
        for (uint32_t i = 0; i < C.size(); ++i) {
            REQUIRE(C(i) == a_val / v);
        }

        // Value of A is not changed
        for (uint32_t i = 0; i < A.size(); ++i) {
            REQUIRE(A(i) == a_val);
        }
    }
    SECTION("multiply with scalar") {
        NDArray<double> A(shape, a_val);
        NDArray<double> B(shape, b_val);
        double v = 3.7;
        auto C = A / v;
        REQUIRE(C.data() != A.data());

        // Value of C matches
        for (uint32_t i = 0; i < C.size(); ++i) {
            REQUIRE(C(i) == a_val / v);
        }

        // Value of A is not changed
        for (uint32_t i = 0; i < A.size(); ++i) {
            REQUIRE(A(i) == a_val);
        }
    }
}

TEST_CASE("Assign an std::array to a 1D NDArray") {
    NDArray<int, 1> a{{5}, 0};
    std::array<int, 5> b{1, 2, 3, 4, 5};
    a = b;
    for (uint32_t i = 0; i < a.size(); ++i) {
        REQUIRE(a(i) == b[i]);
    }
}

TEST_CASE("Assign an std::array to a 1D NDArray of a different size") {
    NDArray<int, 1> a{{3}, 0};
    std::array<int, 5> b{1, 2, 3, 4, 5};
    a = b;

    REQUIRE(a.size() == 5);
    for (uint32_t i = 0; i < a.size(); ++i) {
        REQUIRE(a(i) == b[i]);
    }
}

TEST_CASE("Construct an NDArray from an std::array") {
    std::array<int, 5> b{1, 2, 3, 4, 5};
    NDArray<int, 1> a(b);
    for (uint32_t i = 0; i < a.size(); ++i) {
        REQUIRE(a(i) == b[i]);
    }
}



TEST_CASE("Move construct from an array with Ndim + 1") {
    NDArray<int, 3> a({{1,2,2}}, 0);
    a(0, 0, 0) = 1;
    a(0, 0, 1) = 2;
    a(0, 1, 0) = 3;
    a(0, 1, 1) = 4;


    NDArray<int, 2> b(std::move(a));
    REQUIRE(b.shape() == Shape<2>{2,2});
    REQUIRE(b.size() == 4);
    REQUIRE(b(0, 0) == 1);
    REQUIRE(b(0, 1) == 2);
    REQUIRE(b(1, 0) == 3);
    REQUIRE(b(1, 1) == 4);

}

TEST_CASE("Move construct from an array with Ndim + 1 throws on size mismatch") {
    NDArray<int, 3> a({{2,2,2}}, 0);
    REQUIRE_THROWS(NDArray<int, 2>(std::move(a)));
}

