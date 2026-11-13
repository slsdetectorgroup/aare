// SPDX-License-Identifier: MPL-2.0
#include "aare/NDView.hpp"
#include "aare/NDArray.hpp"
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <vector>

using aare::NDView;
using aare::num_elements;
using aare::Shape;
using aare::ssize_t;

static_assert(std::is_convertible_v<NDView<int, 2>, NDView<const int, 2>>);
static_assert(!std::is_convertible_v<NDView<const int, 2>, NDView<int, 2>>);

TEST_CASE("Calculate size from a shape", "[NDView]") {
    Shape<3> shape{2, 3, 4};
    REQUIRE(num_elements(shape) == 24);

    Shape<2> shape2{5, 5};
    REQUIRE(num_elements(shape2) == 25);

    Shape<1> shape3{10};
    REQUIRE(num_elements(shape3) == 10);

    Shape<3> shape4{1000, 512, 1024};
    REQUIRE(num_elements(shape4) == 524288000);

    // 10GB which is more than INT_MAX bytes
    Shape<3> shape5{10000, 1024, 1024};
    REQUIRE(num_elements(shape5) == 10485760000);
}

TEST_CASE("Element reference 1D", "[NDView]") {
    std::vector<int> vec;
    for (int i = 0; i != 10; ++i) {
        vec.push_back(i);
    }
    NDView<int, 1> data(vec.data(), Shape<1>{10});
    REQUIRE(vec.size() == static_cast<size_t>(data.size()));
    for (int i = 0; i != 10; ++i) {
        REQUIRE(data(i) == vec[i]);
        REQUIRE(data[i] == vec[i]);
    }
}

TEST_CASE("Assign elements through () and []", "[NDView]") {
    std::vector<int> vec;
    for (int i = 0; i != 10; ++i) {
        vec.push_back(i);
    }
    NDView<int, 1> data(vec.data(), Shape<1>{10});
    REQUIRE(vec.size() == static_cast<size_t>(data.size()));

    data[3] = 187;
    data(4) = 512;

    REQUIRE(data(0) == 0);
    REQUIRE(data[0] == 0);
    REQUIRE(data(1) == 1);
    REQUIRE(data[1] == 1);
    REQUIRE(data(2) == 2);
    REQUIRE(data[2] == 2);
    REQUIRE(data(3) == 187);
    REQUIRE(data[3] == 187);
    REQUIRE(data(4) == 512);
    REQUIRE(data[4] == 512);
    REQUIRE(data(5) == 5);
    REQUIRE(data[5] == 5);
    REQUIRE(data(6) == 6);
    REQUIRE(data[6] == 6);
    REQUIRE(data(7) == 7);
    REQUIRE(data[7] == 7);
    REQUIRE(data(8) == 8);
    REQUIRE(data[8] == 8);
    REQUIRE(data(9) == 9);
    REQUIRE(data[9] == 9);
}

TEST_CASE("Element reference 1D with a const NDView", "[NDView]") {
    std::vector<int> vec;
    for (int i = 0; i != 10; ++i) {
        vec.push_back(i);
    }
    const NDView<int, 1> data(vec.data(), Shape<1>{10});
    REQUIRE(vec.size() == static_cast<size_t>(data.size()));
    for (int i = 0; i != 10; ++i) {
        REQUIRE(data(i) == vec[i]);
        REQUIRE(data[i] == vec[i]);
    }
}

TEST_CASE("Convert a mutable NDView to a const NDView", "[NDView]") {
    std::vector<int> vec{1, 2, 3, 4};
    NDView<int, 2> mutable_view(vec.data(), Shape<2>{2, 2});

    NDView<const int, 2> const_view = mutable_view;

    REQUIRE(const_view.data() == mutable_view.data());
    REQUIRE(const_view.shape() == mutable_view.shape());
    REQUIRE(const_view.strides() == mutable_view.strides());
}

TEST_CASE("Element reference 2D", "[NDView]") {
    std::vector<int> vec(12);
    std::iota(vec.begin(), vec.end(), 0);

    NDView<int, 2> data(vec.data(), Shape<2>{3, 4});
    REQUIRE(vec.size() == static_cast<size_t>(data.size()));
    int i = 0;
    for (int row = 0; row != 3; ++row) {
        for (int col = 0; col != 4; ++col) {
            REQUIRE(data(row, col) == i);
            REQUIRE(data[i] == vec[i]);
            ++i;
        }
    }
}

TEST_CASE("Element reference 3D", "[NDView]") {
    std::vector<int> vec;
    for (int i = 0; i != 24; ++i) {
        vec.push_back(i);
    }
    NDView<int, 3> data(vec.data(), Shape<3>{2, 3, 4});
    REQUIRE(vec.size() == static_cast<size_t>(data.size()));
    int i = 0;
    for (int frame = 0; frame != 2; ++frame) {
        for (int row = 0; row != 3; ++row) {
            for (int col = 0; col != 4; ++col) {
                REQUIRE(data(frame, row, col) == i);
                REQUIRE(data[i] == vec[i]);
                ++i;
            }
        }
    }
}

TEST_CASE("Plus and minus with single value", "[NDView]") {
    std::vector<int> vec(12);
    std::iota(vec.begin(), vec.end(), 0);
    NDView<int, 2> data(vec.data(), Shape<2>{3, 4});
    data += 5;
    int i = 0;
    for (int row = 0; row != 3; ++row) {
        for (int col = 0; col != 4; ++col) {
            REQUIRE(data(row, col) == i + 5);
            ++i;
        }
    }
    data -= 3;
    i = 0;
    for (int row = 0; row != 3; ++row) {
        for (int col = 0; col != 4; ++col) {
            REQUIRE(data(row, col) == i + 2);
            ++i;
        }
    }
}

TEST_CASE("Multiply and divide with single value", "[NDView]") {
    std::vector<int> vec;
    for (int i = 0; i != 12; ++i) {
        vec.push_back(i);
    }
    NDView<int, 2> data(vec.data(), Shape<2>{3, 4});
    data *= 5;
    int i = 0;
    for (int row = 0; row != 3; ++row) {
        for (int col = 0; col != 4; ++col) {
            REQUIRE(data(row, col) == i * 5);
            ++i;
        }
    }
    data /= 3;
    i = 0;
    for (int row = 0; row != 3; ++row) {
        for (int col = 0; col != 4; ++col) {
            REQUIRE(data(row, col) == (i * 5) / 3);
            ++i;
        }
    }
}

TEST_CASE("elementwise assign", "[NDView]") {
    std::vector<int> vec(25);
    NDView<int, 2> data(vec.data(), Shape<2>{5, 5});

    data = 3;
    for (auto it : data) {
        REQUIRE(it == 3);
    }
}

TEST_CASE("iterators", "[NDView]") {
    std::vector<int> vec(12);
    std::iota(vec.begin(), vec.end(), 0);
    NDView<int, 1> data(vec.data(), Shape<1>{12});
    int i = 0;
    for (const auto item : data) {
        REQUIRE(item == vec[i]);
        ++i;
    }
    REQUIRE(i == 12);

    for (auto ptr = data.begin(); ptr != data.end(); ++ptr) {
        *ptr += 1;
    }
    for (auto &item : data) {
        ++item;
    }

    i = 0;
    for (const auto item : data) {
        REQUIRE(item == i + 2);
        ++i;
    }
}

TEST_CASE("divide with another NDView", "[NDView]") {
    std::vector<int> vec0{9, 12, 3};
    std::vector<int> vec1{3, 2, 1};
    std::vector<int> result{3, 6, 3};

    NDView<int, 1> data0(vec0.data(),
                         Shape<1>{static_cast<ssize_t>(vec0.size())});
    NDView<int, 1> data1(vec1.data(),
                         Shape<1>{static_cast<ssize_t>(vec1.size())});

    data0 /= data1;

    for (size_t i = 0; i != vec0.size(); ++i) {
        REQUIRE(data0[i] == result[i]);
    }
}

TEST_CASE("Retrieve shape", "[NDView]") {
    std::vector<int> vec(12);
    std::iota(vec.begin(), vec.end(), 0);
    NDView<int, 2> data(vec.data(), Shape<2>{3, 4});
    REQUIRE(data.shape()[0] == 3);
    REQUIRE(data.shape()[1] == 4);
}

TEST_CASE("compare two views", "[NDView]") {
    std::vector<int> vec1(12);
    std::iota(vec1.begin(), vec1.end(), 0);
    NDView<int, 2> view1(vec1.data(), Shape<2>{3, 4});

    std::vector<int> vec2(12);
    std::iota(vec2.begin(), vec2.end(), 0);
    NDView<int, 2> view2(vec2.data(), Shape<2>{3, 4});

    REQUIRE((view1 == view2));
}

TEST_CASE("Compare two views with different size", "[NDView]") {
    std::vector<int> vec1(12);
    std::iota(vec1.begin(), vec1.end(), 0);
    NDView<int, 2> view1(vec1.data(), Shape<2>{3, 4});

    std::vector<int> vec2(8);
    std::iota(vec2.begin(), vec2.end(), 0);
    NDView<int, 2> view2(vec2.data(), Shape<2>{2, 4});

    REQUIRE_FALSE(view1 == view2);
}

TEST_CASE("Compare two views with same size but different shape", "[NDView]") {
    std::vector<int> vec1(12);
    std::iota(vec1.begin(), vec1.end(), 0);
    NDView<int, 2> view1(vec1.data(), Shape<2>{3, 4});

    std::vector<int> vec2(12);
    std::iota(vec2.begin(), vec2.end(), 0);
    NDView<int, 2> view2(vec2.data(), Shape<2>{2, 6});

    REQUIRE_FALSE(view1 == view2);
}

TEST_CASE("Create a view over a vector", "[NDView]") {
    std::vector<int> vec(12);
    std::iota(vec.begin(), vec.end(), 0);
    auto v = aare::make_view(vec);
    REQUIRE(v.shape()[0] == 12);
    REQUIRE(v[0] == 0);
    REQUIRE(v[11] == 11);
}

TEST_CASE("NDView over byte", "[NDView]") {
    std::vector<std::byte> buf(5);
    auto v = aare::make_view(buf);
    REQUIRE(v.shape()[0] == 5);
    REQUIRE(v[0] == std::byte{0});
}

TEST_CASE("NDView subview", "[NDView]") {
    std::vector<int> vec(12);
    std::iota(vec.begin(), vec.end(), 0);
    NDView<int, 2> data(vec.data(), Shape<2>{3, 4});

    auto subview = data.sub_view(1);
    REQUIRE(subview.shape().size() == 1);
    REQUIRE(subview.shape()[0] == 4);
    CHECK(subview(0) == 4);
    CHECK(subview(1) == 5);
    CHECK(subview(2) == 6);
    CHECK(subview(3) == 7);

    REQUIRE_THROWS_AS(data.sub_view(3), std::runtime_error);
    REQUIRE_THROWS_AS(data.sub_view(-1), std::runtime_error);
}

TEST_CASE("Index operators shared between NDView and NDArray") {
    // Both get operator()(i,j,...) and operator[] from NDOps, adding
    // it must not grow the view
    static_assert(sizeof(NDView<int, 2>) ==
                  sizeof(int *) + 4 * sizeof(ssize_t) + sizeof(uint64_t));

    aare::NDArray<int, 3> arr({2, 3, 4}, 0);
    auto view = arr.view();

    // Write through the view with indices of mixed integer types
    view(1, size_t{2}, ssize_t{3}) = 57;
    view[0] = 5;
    REQUIRE(arr(1, 2, 3) == 57);
    REQUIRE(arr[23] == 57);
    REQUIRE(arr(0, 0, 0) == 5);

    // A single index on a view with more dimensions gives a view, not an
    // element
    auto frame = view(1);
    static_assert(std::is_same_v<decltype(frame), NDView<int, 2>>);
    REQUIRE(frame.shape() == aare::Shape<2>{3, 4});
    REQUIRE(frame(2, 3) == 57);
    frame(0, 1) = 9;
    REQUIRE(arr(1, 0, 1) == 9);

    // Const access returns const references
    const auto &carr = arr;
    const auto &cview = view;
    static_assert(std::is_same_v<decltype(carr(0, 0, 0)), const int &>);
    static_assert(std::is_same_v<decltype(cview(0, 0, 0)), const int &>);
    static_assert(std::is_same_v<decltype(carr[0]), const int &>);
    static_assert(std::is_same_v<decltype(cview[0]), const int &>);
    static_assert(std::is_same_v<decltype(arr.view()(0, 0, 0)), int &>);
    static_assert(std::is_same_v<decltype(carr.view()(0, 0, 0)), const int &>);
    REQUIRE(cview(1, 2, 3) == 57);
}

TEST_CASE("Elementwise math shared between NDView and NDArray") {
    aare::NDArray<int, 2> a({2, 3}, 6);
    aare::NDArray<int, 2> b({2, 3}, 2);
    auto va = a.view();

    SECTION("scalar operators on a view modify the underlying array") {
        va += 2;   // 8
        va -= 1;   // 7
        va *= 3;   // 21
        va /= 2;   // 10
        va &= 0x6; // 2
        for (auto v : a)
            REQUIRE(v == 2);
    }

    SECTION("arrays and views can be mixed") {
        va += b; // view op array
        REQUIRE(a(1, 2) == 8);
        a -= b.view(); // array op view
        REQUIRE(a(1, 2) == 6);
        va *= b.view(); // view op view
        REQUIRE(a(1, 2) == 12);
        const auto &cb = b;
        a /= cb.view(); // array op read only view
        REQUIRE(a(1, 2) == 6);
    }

    SECTION("the right hand side can have another element type") {
        aare::NDArray<double, 2> d({2, 3}, 1.5);
        a *= d;
        REQUIRE(a(0, 0) == 9);
        va /= d.view();
        REQUIRE(a(0, 0) == 6);
    }

    SECTION("an expression is applied without changing its operands") {
        a += b * b;
        REQUIRE(a(0, 0) == 10);
        va -= b + b;
        REQUIRE(a(1, 2) == 6);
        a += a * 2; // reads and writes the same buffer
        REQUIRE(a(1, 2) == 18);
        REQUIRE(b(0, 0) == 2);
    }

    SECTION("shape mismatch throws for arrays and views") {
        aare::NDArray<int, 2> c({3, 2}, 1);
        REQUIRE_THROWS_AS(a += c, std::runtime_error);
        REQUIRE_THROWS_AS(va /= c.view(), std::runtime_error);
        REQUIRE_THROWS_AS(va *= c, std::runtime_error);
        REQUIRE(a(0, 0) == 6);
    }

    SECTION("comparison") {
        aare::NDArray<int, 2> c({2, 3}, 6);
        REQUIRE(va == c.view());
        REQUIRE_FALSE(va != c.view());
        REQUIRE(a == c);
        c(1, 1) = 0;
        REQUIRE(va != c.view());
        REQUIRE(a != c);
        // same data, different shape
        aare::NDArray<int, 2> e({3, 2}, 6);
        REQUIRE(a != e);
        REQUIRE(va != e.view());
    }

    SECTION("stream output is the same for an array and its view") {
        std::ostringstream from_array;
        std::ostringstream from_view;
        from_array << a;
        from_view << va;
        REQUIRE(from_array.str() == "  6   6   6 \n  6   6   6 \n");
        REQUIRE(from_view.str() == from_array.str());
    }
}
