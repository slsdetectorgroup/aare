#include "aare/InclusiveROI.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace aare;

// translate
TEST_CASE("translate ROI", "[InclusiveROIgeometry]") {
    const InclusiveROI roi{10, 19, 20, 29};

    SECTION("positive shift") {
        auto result = inclusiveroi::geom::translate(roi, 5, 7);

        CHECK(result == InclusiveROI{15, 24, 27, 36});
    }

    SECTION("negative shift") {
        auto result = inclusiveroi::geom::translate(roi, -5, -7);

        CHECK(result == aare::InclusiveROI{5, 14, 13, 22});
    }

    SECTION("zero shift") {
        CHECK(inclusiveroi::geom::translate(roi, 0, 0) == roi);
    }
}

// mirror
TEST_CASE("mirror_on_y mirrors ROI about vertical axis",
          "[InclusiveROIgeometry]") {

    SECTION("ROI on left side of axis") {
        InclusiveROI roi{1, 3, 10, 20};

        auto mirrored = inclusiveroi::geom::mirror_on_y(roi, 5);

        CHECK(mirrored == InclusiveROI{6, 8, 10, 20});
    }

    SECTION("ROI right of axis") {
        InclusiveROI roi{6, 8, 10, 20};

        CHECK(inclusiveroi::geom::mirror_on_y(roi, 5) ==
              InclusiveROI{1, 3, 10, 20});
    }

    SECTION("ROI touching the axis") {
        InclusiveROI roi{4, 5, 10, 20};

        CHECK(inclusiveroi::geom::mirror_on_y(roi, 5) ==
              InclusiveROI{4, 5, 10, 20});
    }

    SECTION("mirroring twice restores original ROI") {
        InclusiveROI roi{2, 12, 10, 20};

        auto mirrored = inclusiveroi::geom::mirror_on_y(roi, 8);
        auto restored = inclusiveroi::geom::mirror_on_y(mirrored, 8);

        CHECK(restored == roi);
    }

    SECTION("even-width base ROI") {
        InclusiveROI base_roi{0, 11, 10, 20};
        InclusiveROI roi{3, 7, 12, 18};

        CHECK(inclusiveroi::geom::mirror_on_y(roi, base_roi.width() / 2) ==
              InclusiveROI{4, 8, 12, 18});
    }

    SECTION("odd-width base ROI") {
        InclusiveROI base_roi{0, 12, 10, 20};
        InclusiveROI roi{3, 7, 12, 18};

        CHECK(inclusiveroi::geom::mirror_on_y(roi, base_roi.width() / 2) ==
              InclusiveROI{4, 8, 12, 18});
    }
}

TEST_CASE("mirror_on_x mirrors ROI about horizontal axis",
          "[InclusiveROIgeometry]") {

    SECTION("ROI below axis") {
        InclusiveROI roi{10, 20, 1, 3};

        auto mirrored = inclusiveroi::geom::mirror_on_x(roi, 5);

        CHECK(mirrored == InclusiveROI{10, 20, 6, 8});
    }

    SECTION("ROI above axis") {
        InclusiveROI roi{10, 20, 6, 8};

        CHECK(inclusiveroi::geom::mirror_on_x(roi, 5) ==
              InclusiveROI{10, 20, 1, 3});
    }

    SECTION("ROI touching the axis") {
        InclusiveROI roi{10, 20, 4, 5};

        CHECK(inclusiveroi::geom::mirror_on_x(roi, 5) ==
              InclusiveROI{10, 20, 4, 5});
    }

    SECTION("mirroring twice restores original ROI") {
        InclusiveROI roi{10, 20, 2, 12};

        auto mirrored = inclusiveroi::geom::mirror_on_x(roi, 8);
        auto restored = inclusiveroi::geom::mirror_on_x(mirrored, 8);

        CHECK(restored == roi);
    }

    SECTION("even-height base ROI") {
        InclusiveROI base_roi{10, 20, 0, 11};
        InclusiveROI roi{12, 18, 3, 7};

        CHECK(inclusiveroi::geom::mirror_on_x(roi, base_roi.height() / 2) ==
              InclusiveROI{12, 18, 4, 8});
    }

    SECTION("odd-height base ROI") {
        InclusiveROI base_roi{10, 20, 0, 12};
        InclusiveROI roi{12, 18, 3, 7};

        CHECK(inclusiveroi::geom::mirror_on_x(roi, base_roi.height() / 2) ==
              InclusiveROI{12, 18, 4, 8});
    }
}

// intersect
TEST_CASE("intersect ROIs", "[InclusiveROIgeometry]") {
    SECTION("partially overlapping") {
        InclusiveROI a{0, 9, 0, 9};
        InclusiveROI b{5, 14, 3, 7};

        CHECK(inclusiveroi::geom::intersect(a, b) == InclusiveROI{5, 9, 3, 7});
    }

    SECTION("one ROI inside the other") {
        InclusiveROI a{0, 20, 0, 20};
        InclusiveROI b{5, 10, 7, 12};

        CHECK(inclusiveroi::geom::intersect(a, b) == b);
    }

    SECTION("identical ROIs") {
        InclusiveROI a{5, 10, 7, 12};

        CHECK(inclusiveroi::geom::intersect(a, a) == a);
    }

    SECTION("no overlap") {
        InclusiveROI a{0, 9, 0, 9};
        InclusiveROI b{10, 19, 0, 9};

        CHECK(inclusiveroi::geom::intersect(a, b).is_empty());
    }
}

// rebase
TEST_CASE("rebase ROI", "[InclusiveROIgeometry]") {
    SECTION("base ROI origin becomes (0,0) in the rebased coordinate system") {
        InclusiveROI input{110, 119, 220, 229};
        InclusiveROI base{100, 199, 200, 299};

        CHECK(inclusiveroi::geom::rebaseROI(input, base) ==
              InclusiveROI{10, 19, 20, 29});
    }

    SECTION("input larger than base") {
        InclusiveROI input{90, 219, 80, 329};
        InclusiveROI base{100, 199, 200, 299};

        CHECK(inclusiveroi::geom::rebaseROI(input, base) ==
              InclusiveROI{-10, 119, -120, 129});
    }

    SECTION("input outside of base") {
        InclusiveROI input{10, 80, 80, 129};
        InclusiveROI base{100, 199, 200, 299};

        CHECK(inclusiveroi::geom::rebaseROI(input, base) ==
              InclusiveROI{-90, -20, -120, -71});
    }

    SECTION("rebasing base itself produces origin") {
        InclusiveROI base{100, 199, 200, 299};

        CHECK(inclusiveroi::geom::rebaseROI(base, base) ==
              InclusiveROI{0, 99, 0, 99});
    }
}

// unite
TEST_CASE("unite ROIs", "[InclusiveROIgeometry]") {
    SECTION("overlapping horizontally") {
        InclusiveROI a{0, 9, 0, 9};
        InclusiveROI b{5, 14, 0, 9};

        CHECK(inclusiveroi::geom::unite(a, b) == InclusiveROI{0, 14, 0, 9});
    }

    SECTION("adjacent horizontally") {
        InclusiveROI a{0, 9, 0, 9};
        InclusiveROI b{10, 19, 0, 9};

        CHECK(inclusiveroi::geom::unite(a, b) == InclusiveROI{0, 19, 0, 9});
    }

    SECTION("overlapping vertically") {
        InclusiveROI a{0, 9, 0, 9};
        InclusiveROI b{0, 9, 5, 14};

        CHECK(inclusiveroi::geom::unite(a, b) == InclusiveROI{0, 9, 0, 14});
    }

    SECTION("adjacent vertically") {
        InclusiveROI a{0, 9, 0, 9};
        InclusiveROI b{0, 9, 10, 19};

        CHECK(inclusiveroi::geom::unite(a, b) == InclusiveROI{0, 9, 0, 19});
    }

    SECTION("non-contiguous ROIs throw") {
        InclusiveROI a{0, 9, 0, 9};
        InclusiveROI b{20, 29, 0, 9};

        CHECK_THROWS_AS(inclusiveroi::geom::unite(a, b), std::runtime_error);
    }

    SECTION("different y extents throw") {
        InclusiveROI a{0, 9, 0, 9};
        InclusiveROI b{5, 14, 1, 9};

        CHECK_THROWS_AS(inclusiveroi::geom::unite(a, b), std::runtime_error);
    }
}