#include "aare/NDView.hpp"
#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include <numeric>
#include <vector>

using aare::NDView;
using aare::Shape;

TEST_CASE("Element reference 1D") {
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


TEST_CASE("Assign elements through () and []") {
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

TEST_CASE("Element reference 1D with a const NDView") {
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


TEST_CASE("Element reference 2D") {
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

TEST_CASE("Element reference 3D") {
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

TEST_CASE("Plus and minus with single value") {
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

TEST_CASE("Multiply and divide with single value") {
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

TEST_CASE("elementwise assign") {
    std::vector<int> vec(25);
    NDView<int, 2> data(vec.data(), Shape<2>{5, 5});

    data = 3;
    for (auto it : data) {
        REQUIRE(it == 3);
    }
}

TEST_CASE("iterators") {
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



TEST_CASE("divide with another NDView") {
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

TEST_CASE("Retrieve shape") {
    std::vector<int> vec(12);
    std::iota(vec.begin(), vec.end(), 0);
    NDView<int, 2> data(vec.data(), Shape<2>{3, 4});
    REQUIRE(data.shape()[0] == 3);
    REQUIRE(data.shape()[1] == 4);
}

TEST_CASE("compare two views") {
    std::vector<int> vec1(12);
    std::iota(vec1.begin(), vec1.end(), 0);
    NDView<int, 2> view1(vec1.data(), Shape<2>{3, 4});

    std::vector<int> vec2(12);
    std::iota(vec2.begin(), vec2.end(), 0);
    NDView<int, 2> view2(vec2.data(), Shape<2>{3, 4});

    REQUIRE((view1 == view2));
}

TEST_CASE("Compare two views with different size"){
    std::vector<int> vec1(12);
    std::iota(vec1.begin(), vec1.end(), 0);
    NDView<int, 2> view1(vec1.data(), Shape<2>{3, 4});

    std::vector<int> vec2(8);
    std::iota(vec2.begin(), vec2.end(), 0);
    NDView<int, 2> view2(vec2.data(), Shape<2>{2, 4});

    REQUIRE_FALSE(view1 == view2);
}

TEST_CASE("Compare two views with same size but different shape"){
    std::vector<int> vec1(12);
    std::iota(vec1.begin(), vec1.end(), 0);
    NDView<int, 2> view1(vec1.data(), Shape<2>{3, 4});

    std::vector<int> vec2(12);
    std::iota(vec2.begin(), vec2.end(), 0);
    NDView<int, 2> view2(vec2.data(), Shape<2>{2, 6});

    REQUIRE_FALSE(view1 == view2);
}

TEST_CASE("Create a view over a vector") {
    std::vector<int> vec(12);
    std::iota(vec.begin(), vec.end(), 0);
    auto v = aare::make_view(vec);
    REQUIRE(v.shape()[0] == 12);
    REQUIRE(v[0] == 0);
    REQUIRE(v[11] == 11);
}