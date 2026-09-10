// SPDX-License-Identifier: MPL-2.0
#include "aare/ClusterFile.hpp"
#include "test_config.hpp"

#include "aare/defs.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

using aare::Cluster;
using aare::ClusterFile;
using aare::ClusterVector;

namespace {

class TemporaryClusterFile {
  public:
    TemporaryClusterFile() {
        const auto unique =
            std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path() /
                 ("aare-cluster-file-" + std::to_string(unique) + ".clust");
    }

    TemporaryClusterFile(const TemporaryClusterFile &) = delete;
    TemporaryClusterFile &operator=(const TemporaryClusterFile &) = delete;

    ~TemporaryClusterFile() {
        std::error_code error;
        std::filesystem::remove(m_path, error);
    }

    const std::filesystem::path &path() const { return m_path; }

  private:
    std::filesystem::path m_path;
};

using TestCluster = Cluster<double, 3, 3>;

ClusterVector<TestCluster> make_test_frame(int32_t frame_number,
                                           double offset) {
    ClusterVector<TestCluster> clusters(2, frame_number);
    clusters.push_back(
        TestCluster{5,
                    6,
                    {offset, offset + 1, offset + 2, offset + 3, offset + 4,
                     offset + 5, offset + 6, offset + 7, offset + 8}});
    clusters.push_back(TestCluster{7,
                                   8,
                                   {offset + 9, offset + 10, offset + 11,
                                    offset + 12, offset + 13, offset + 14,
                                    offset + 15, offset + 16, offset + 17}});
    return clusters;
}

void check_frame(const ClusterVector<TestCluster> &actual,
                 const ClusterVector<TestCluster> &expected) {
    REQUIRE(actual.frame_number() == expected.frame_number());
    REQUIRE(actual.size() == expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        CHECK(actual[i].x == expected[i].x);
        CHECK(actual[i].y == expected[i].y);
        CHECK(actual[i].data == expected[i].data);
    }
}

} // namespace

TEST_CASE("Read one frame from a cluster file", "[.with-data]") {
    // We know that the frame has 97 clusters
    auto fpath = test_data_path() / "clust" / "single_frame_97_clustrers.clust";
    REQUIRE(std::filesystem::exists(fpath));

    ClusterFile<Cluster<int32_t, 3, 3>> f(fpath);
    CHECK(f.estimate_n_clusters() == 97);
    CHECK(f.tell() == 0);
    auto clusters = f.read_frame();
    REQUIRE(clusters);
    CHECK(clusters->size() == 97);
    CHECK(clusters->frame_number() == 135);
    CHECK((*clusters)[0].x == 1);
    CHECK((*clusters)[0].y == 200);
    int32_t expected_cluster_data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(std::equal(std::begin((*clusters)[0].data),
                     std::end((*clusters)[0].data),
                     std::begin(expected_cluster_data)));
}

TEST_CASE("Read one frame using ROI", "[.with-data]") {
    // We know that the frame has 97 clusters
    auto fpath = test_data_path() / "clust" / "single_frame_97_clustrers.clust";
    REQUIRE(std::filesystem::exists(fpath));

    ClusterFile<Cluster<int32_t, 3, 3>> f(fpath);
    aare::ROI roi;
    roi.xmin = 0;
    roi.xmax = 50;
    roi.ymin = 200;
    roi.ymax = 249;
    f.set_roi(roi);
    auto clusters = f.read_frame();
    REQUIRE(clusters);
    REQUIRE(clusters->size() == 49);
    REQUIRE(clusters->frame_number() == 135);

    // Check that all clusters are within the ROI
    for (size_t i = 0; i < clusters->size(); i++) {
        auto c = (*clusters)[i];
        REQUIRE(c.x >= roi.xmin);
        REQUIRE(c.x <= roi.xmax);
        REQUIRE(c.y >= roi.ymin);
        REQUIRE(c.y <= roi.ymax);
    }

    CHECK((*clusters)[0].x == 1);
    CHECK((*clusters)[0].y == 200);
    int32_t expected_cluster_data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(std::equal(std::begin((*clusters)[0].data),
                     std::end((*clusters)[0].data),
                     std::begin(expected_cluster_data)));
}

TEST_CASE("Read clusters from single frame file", "[.with-data]") {

    //  frame_number, num_clusters   [135] 97
    // [  1 200] [0 1 2 3 4 5 6 7 8]
    // [  2 201] [ 9 10 11 12 13 14 15 16 17]
    // [  3 202] [18 19 20 21 22 23 24 25 26]
    // [  4 203] [27 28 29 30 31 32 33 34 35]
    // [  5 204] [36 37 38 39 40 41 42 43 44]
    // [  6 205] [45 46 47 48 49 50 51 52 53]
    // [  7 206] [54 55 56 57 58 59 60 61 62]
    // [  8 207] [63 64 65 66 67 68 69 70 71]
    // [  9 208] [72 73 74 75 76 77 78 79 80]
    // [ 10 209] [81 82 83 84 85 86 87 88 89]
    // [ 11 210] [90 91 92 93 94 95 96 97 98]
    // [ 12 211] [ 99 100 101 102 103 104 105 106 107]
    // [ 13 212] [108 109 110 111 112 113 114 115 116]
    // [ 14 213] [117 118 119 120 121 122 123 124 125]
    // [ 15 214] [126 127 128 129 130 131 132 133 134]
    // [ 16 215] [135 136 137 138 139 140 141 142 143]
    // [ 17 216] [144 145 146 147 148 149 150 151 152]
    // [ 18 217] [153 154 155 156 157 158 159 160 161]
    // [ 19 218] [162 163 164 165 166 167 168 169 170]
    // [ 20 219] [171 172 173 174 175 176 177 178 179]
    // [ 21 220] [180 181 182 183 184 185 186 187 188]
    // [ 22 221] [189 190 191 192 193 194 195 196 197]
    // [ 23 222] [198 199 200 201 202 203 204 205 206]
    // [ 24 223] [207 208 209 210 211 212 213 214 215]
    // [ 25 224] [216 217 218 219 220 221 222 223 224]
    // [ 26 225] [225 226 227 228 229 230 231 232 233]
    // [ 27 226] [234 235 236 237 238 239 240 241 242]
    // [ 28 227] [243 244 245 246 247 248 249 250 251]
    // [ 29 228] [252 253 254 255 256 257 258 259 260]
    // [ 30 229] [261 262 263 264 265 266 267 268 269]
    // [ 31 230] [270 271 272 273 274 275 276 277 278]
    // [ 32 231] [279 280 281 282 283 284 285 286 287]
    // [ 33 232] [288 289 290 291 292 293 294 295 296]
    // [ 34 233] [297 298 299 300 301 302 303 304 305]
    // [ 35 234] [306 307 308 309 310 311 312 313 314]
    // [ 36 235] [315 316 317 318 319 320 321 322 323]
    // [ 37 236] [324 325 326 327 328 329 330 331 332]
    // [ 38 237] [333 334 335 336 337 338 339 340 341]
    // [ 39 238] [342 343 344 345 346 347 348 349 350]
    // [ 40 239] [351 352 353 354 355 356 357 358 359]
    // [ 41 240] [360 361 362 363 364 365 366 367 368]
    // [ 42 241] [369 370 371 372 373 374 375 376 377]
    // [ 43 242] [378 379 380 381 382 383 384 385 386]
    // [ 44 243] [387 388 389 390 391 392 393 394 395]
    // [ 45 244] [396 397 398 399 400 401 402 403 404]
    // [ 46 245] [405 406 407 408 409 410 411 412 413]
    // [ 47 246] [414 415 416 417 418 419 420 421 422]
    // [ 48 247] [423 424 425 426 427 428 429 430 431]
    // [ 49 248] [432 433 434 435 436 437 438 439 440]
    // [ 50 249] [441 442 443 444 445 446 447 448 449]
    // [ 51 250] [450 451 452 453 454 455 456 457 458]
    // [ 52 251] [459 460 461 462 463 464 465 466 467]
    // [ 53 252] [468 469 470 471 472 473 474 475 476]
    // [ 54 253] [477 478 479 480 481 482 483 484 485]
    // [ 55 254] [486 487 488 489 490 491 492 493 494]
    // [ 56 255] [495 496 497 498 499 500 501 502 503]
    // [ 57 256] [504 505 506 507 508 509 510 511 512]
    // [ 58 257] [513 514 515 516 517 518 519 520 521]
    // [ 59 258] [522 523 524 525 526 527 528 529 530]
    // [ 60 259] [531 532 533 534 535 536 537 538 539]
    // [ 61 260] [540 541 542 543 544 545 546 547 548]
    // [ 62 261] [549 550 551 552 553 554 555 556 557]
    // [ 63 262] [558 559 560 561 562 563 564 565 566]
    // [ 64 263] [567 568 569 570 571 572 573 574 575]
    // [ 65 264] [576 577 578 579 580 581 582 583 584]
    // [ 66 265] [585 586 587 588 589 590 591 592 593]
    // [ 67 266] [594 595 596 597 598 599 600 601 602]
    // [ 68 267] [603 604 605 606 607 608 609 610 611]
    // [ 69 268] [612 613 614 615 616 617 618 619 620]
    // [ 70 269] [621 622 623 624 625 626 627 628 629]
    // [ 71 270] [630 631 632 633 634 635 636 637 638]
    // [ 72 271] [639 640 641 642 643 644 645 646 647]
    // [ 73 272] [648 649 650 651 652 653 654 655 656]
    // [ 74 273] [657 658 659 660 661 662 663 664 665]
    // [ 75 274] [666 667 668 669 670 671 672 673 674]
    // [ 76 275] [675 676 677 678 679 680 681 682 683]
    // [ 77 276] [684 685 686 687 688 689 690 691 692]
    // [ 78 277] [693 694 695 696 697 698 699 700 701]
    // [ 79 278] [702 703 704 705 706 707 708 709 710]
    // [ 80 279] [711 712 713 714 715 716 717 718 719]
    // [ 81 280] [720 721 722 723 724 725 726 727 728]
    // [ 82 281] [729 730 731 732 733 734 735 736 737]
    // [ 83 282] [738 739 740 741 742 743 744 745 746]
    // [ 84 283] [747 748 749 750 751 752 753 754 755]
    // [ 85 284] [756 757 758 759 760 761 762 763 764]
    // [ 86 285] [765 766 767 768 769 770 771 772 773]
    // [ 87 286] [774 775 776 777 778 779 780 781 782]
    // [ 88 287] [783 784 785 786 787 788 789 790 791]
    // [ 89 288] [792 793 794 795 796 797 798 799 800]
    // [ 90 289] [801 802 803 804 805 806 807 808 809]
    // [ 91 290] [810 811 812 813 814 815 816 817 818]
    // [ 92 291] [819 820 821 822 823 824 825 826 827]
    // [ 93 292] [828 829 830 831 832 833 834 835 836]
    // [ 94 293] [837 838 839 840 841 842 843 844 845]
    // [ 95 294] [846 847 848 849 850 851 852 853 854]
    // [ 96 295] [855 856 857 858 859 860 861 862 863]
    // [ 97 296] [864 865 866 867 868 869 870 871 872]

    auto fpath = test_data_path() / "clust" / "single_frame_97_clustrers.clust";

    REQUIRE(std::filesystem::exists(fpath));

    SECTION("Read fewer clusters than available") {
        ClusterFile<Cluster<int32_t, 3, 3>> f(fpath);
        auto clusters = f.read_clusters(50);
        REQUIRE(clusters.size() == 50);
        REQUIRE(clusters.frame_number() == 135);
        int32_t expected_cluster_data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
        REQUIRE(clusters[0].x == 1);
        REQUIRE(clusters[0].y == 200);
        CHECK(std::equal(std::begin(clusters[0].data),
                         std::end(clusters[0].data),
                         std::begin(expected_cluster_data)));
    }
    SECTION("Read more clusters than available") {
        ClusterFile<Cluster<int32_t, 3, 3>> f(fpath);
        // 100 is the maximum number of clusters read
        auto clusters = f.read_clusters(100);
        REQUIRE(clusters.size() == 97);
        REQUIRE(clusters.frame_number() == 135);
        int32_t expected_cluster_data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
        REQUIRE(clusters[0].x == 1);
        REQUIRE(clusters[0].y == 200);
        CHECK(std::equal(std::begin(clusters[0].data),
                         std::end(clusters[0].data),
                         std::begin(expected_cluster_data)));
    }
    SECTION("Read all clusters") {
        ClusterFile<Cluster<int32_t, 3, 3>> f(fpath);
        auto clusters = f.read_clusters(97);
        REQUIRE(clusters.size() == 97);
        REQUIRE(clusters.frame_number() == 135);
        REQUIRE(clusters[0].x == 1);
        REQUIRE(clusters[0].y == 200);
        int32_t expected_cluster_data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
        CHECK(std::equal(std::begin(clusters[0].data),
                         std::end(clusters[0].data),
                         std::begin(expected_cluster_data)));
    }
}

TEST_CASE("Read clusters from single frame file with ROI", "[.with-data]") {
    auto fpath = test_data_path() / "clust" / "single_frame_97_clustrers.clust";
    REQUIRE(std::filesystem::exists(fpath));

    ClusterFile<Cluster<int32_t, 3, 3>> f(fpath);

    aare::ROI roi;
    roi.xmin = 0;
    roi.xmax = 50;
    roi.ymin = 200;
    roi.ymax = 249;
    f.set_roi(roi);

    auto clusters = f.read_clusters(10);

    CHECK(clusters.size() == 10);
    CHECK(clusters.frame_number() == 135);
    CHECK(clusters[0].x == 1);
    CHECK(clusters[0].y == 200);
    int32_t expected_cluster_data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(std::equal(std::begin(clusters[0].data), std::end(clusters[0].data),
                     std::begin(expected_cluster_data)));
}

TEST_CASE("Read cluster from multiple frame file", "[.with-data]") {

    using ClusterType = Cluster<double, 2, 2>;

    auto fpath =
        test_data_path() / "clust" / "Two_frames_2x2double_test_clusters.clust";

    REQUIRE(std::filesystem::exists(fpath));

    // Two_frames_2x2double_test_clusters.clust
    //  frame number, num_clusters 0, 4
    //[10, 20], {0. ,0., 0., 0.}
    //[11, 30], {1., 1., 1., 1.}
    //[12, 40], {2., 2., 2., 2.}
    //[13, 50], {3., 3., 3., 3.}
    //  1,4
    //[10, 20], {4., 4., 4., 4.}
    //[11, 30], {5., 5., 5., 5.}
    //[12, 40], {6., 6., 6., 6.}
    //[13, 50], {7., 7., 7., 7.}

    SECTION("Read clusters from both frames") {
        ClusterFile<ClusterType> f(fpath);
        CHECK(f.estimate_n_clusters() == 8);
        CHECK(f.tell() == 0);
        auto clusters = f.read_clusters(2);
        REQUIRE(clusters.size() == 2);
        REQUIRE(clusters.frame_number() == 0);

        auto clusters1 = f.read_clusters(3);

        REQUIRE(clusters1.size() == 3);
        REQUIRE(clusters1.frame_number() == 1);
    }

    SECTION("Read all clusters") {
        ClusterFile<ClusterType> f(fpath);
        auto clusters = f.read_clusters(8);
        REQUIRE(clusters.size() == 8);
        REQUIRE(clusters.frame_number() == 1);
    }

    SECTION("Read clusters from one frame") {
        ClusterFile<ClusterType> f(fpath);
        auto clusters = f.read_clusters(2);
        REQUIRE(clusters.size() == 2);
        REQUIRE(clusters.frame_number() == 0);

        auto clusters1 = f.read_clusters(1);

        REQUIRE(clusters1.size() == 1);
        REQUIRE(clusters1.frame_number() == 0);
    }
}

TEST_CASE("ClusterFile flushes a frame when the writer is destroyed",
          "[ClusterFile]") {
    TemporaryClusterFile file;
    auto expected = make_test_frame(42, 0.0);

    {
        ClusterFile<TestCluster> writer(file.path(), 1000, "w");
        writer.write_frame(expected);
    }

    const auto expected_file_size = sizeof(int32_t) + sizeof(uint32_t) +
                                    expected.size() * sizeof(TestCluster);
    CHECK(std::filesystem::file_size(file.path()) == expected_file_size);

    ClusterFile<TestCluster> reader(file.path());
    auto actual = reader.read_frame();
    REQUIRE(actual);
    check_frame(*actual, expected);
}

TEST_CASE("ClusterFile appends frames", "[ClusterFile]") {
    TemporaryClusterFile file;
    auto first_expected = make_test_frame(42, 0.0);
    auto second_expected = make_test_frame(43, 100.0);

    {
        ClusterFile<TestCluster> writer(file.path(), 1000, "w");
        writer.write_frame(first_expected);
    }
    {
        ClusterFile<TestCluster> writer(file.path(), 1000, "a");
        writer.write_frame(second_expected);
    }

    ClusterFile<TestCluster> reader(file.path());
    auto first_actual = reader.read_frame();
    auto second_actual = reader.read_frame();
    REQUIRE(first_actual);
    REQUIRE(second_actual);
    check_frame(*first_actual, first_expected);
    check_frame(*second_actual, second_expected);
}

TEST_CASE("ClusterFile reads frames into an existing cluster vector",
          "[ClusterFile]") {
    TemporaryClusterFile file;
    auto first_expected = make_test_frame(42, 0.0);
    auto second_expected = make_test_frame(43, 100.0);

    {
        ClusterFile<TestCluster> writer(file.path(), 1000, "w");
        writer.write_frame(first_expected);
        writer.write_frame(second_expected);
    }

    ClusterFile<TestCluster> reader(file.path());
    ClusterVector<TestCluster> actual(2);
    const auto initial_data = actual.data();

    REQUIRE(reader.read_frame(actual));
    CHECK(actual.data() == initial_data);
    check_frame(actual, first_expected);

    REQUIRE(reader.read_frame(actual));
    CHECK(actual.data() == initial_data);
    check_frame(actual, second_expected);

    CHECK_FALSE(reader.read_frame(actual));
    check_frame(actual, second_expected);
}

TEST_CASE("ClusterFile reports a clean end of file", "[ClusterFile]") {
    TemporaryClusterFile file;
    {
        ClusterFile<TestCluster> writer(file.path(), 1000, "w");
    }

    ClusterFile<TestCluster> reader(file.path());
    ClusterVector<TestCluster> clusters;
    CHECK_FALSE(reader.read_frame(clusters));
    CHECK(clusters.empty());
    CHECK_FALSE(reader.read_frame().has_value());
}

TEST_CASE("ClusterFile reports complete frames with no output clusters",
          "[ClusterFile]") {
    TemporaryClusterFile file;
    ClusterVector<TestCluster> empty_frame(0, 41);
    auto expected = make_test_frame(42, 0.0);
    {
        ClusterFile<TestCluster> writer(file.path(), 1000, "w");
        writer.write_frame(empty_frame);
        writer.write_frame(expected);
    }

    ClusterFile<TestCluster> reader(file.path());
    ClusterVector<TestCluster> clusters;
    auto empty_actual = reader.read_frame();
    REQUIRE(empty_actual);
    CHECK(empty_actual->empty());
    CHECK(empty_actual->frame_number() == 41);

    aare::ROI roi;
    roi.xmin = 100;
    roi.xmax = 200;
    roi.ymin = 100;
    roi.ymax = 200;
    reader.set_roi(roi);

    REQUIRE(reader.read_frame(clusters));
    CHECK(clusters.empty());
    CHECK(clusters.frame_number() == expected.frame_number());
    CHECK_FALSE(reader.read_frame(clusters));
}

TEST_CASE("ClusterFile rejects incomplete frames", "[ClusterFile]") {
    TemporaryClusterFile file;
    auto expected = make_test_frame(42, 0.0);
    {
        ClusterFile<TestCluster> writer(file.path(), 1000, "w");
        writer.write_frame(expected);
    }

    SECTION("incomplete frame number") {
        std::filesystem::resize_file(file.path(), sizeof(int32_t) - 1);
    }
    SECTION("incomplete cluster count") {
        std::filesystem::resize_file(file.path(),
                                     sizeof(int32_t) + sizeof(uint32_t) - 1);
    }
    SECTION("incomplete cluster data") {
        std::filesystem::resize_file(
            file.path(), std::filesystem::file_size(file.path()) - 1);
    }

    {
        ClusterFile<TestCluster> reader(file.path());
        ClusterVector<TestCluster> clusters;
        CHECK_THROWS_AS(reader.read_frame(clusters), std::runtime_error);
    }
    {
        ClusterFile<TestCluster> reader(file.path());
        CHECK_THROWS_AS(reader.read_frame(), std::runtime_error);
    }
}

TEST_CASE("ClusterFile close is idempotent", "[ClusterFile]") {
    TemporaryClusterFile file;
    auto expected = make_test_frame(42, 0.0);

    ClusterFile<TestCluster> writer(file.path(), 1000, "w");
    writer.write_frame(expected);
    writer.close();

    CHECK_NOTHROW(writer.close());
    CHECK_THROWS_AS(writer.tell(), std::runtime_error);
    CHECK_THROWS_AS(writer.write_frame(expected), std::runtime_error);

    ClusterFile<TestCluster> reader(file.path());
    auto actual = reader.read_frame();
    REQUIRE(actual);
    check_frame(*actual, expected);
}

TEST_CASE("ClusterFile chunk reads reject incomplete frames", "[ClusterFile]") {
    const auto use_roi = GENERATE(false, true);
    CAPTURE(use_roi);
    TemporaryClusterFile file;
    auto expected = make_test_frame(42, 0.0);
    {
        ClusterFile<TestCluster> writer(file.path(), 1000, "w");
        writer.write_frame(expected);
    }

    SECTION("incomplete frame number") {
        std::filesystem::resize_file(file.path(), sizeof(int32_t) - 1);
    }
    SECTION("missing cluster count") {
        std::filesystem::resize_file(file.path(), sizeof(int32_t));
    }
    SECTION("incomplete cluster count") {
        std::filesystem::resize_file(file.path(),
                                     sizeof(int32_t) + sizeof(uint32_t) - 1);
    }
    SECTION("missing cluster record") {
        std::filesystem::resize_file(file.path(), sizeof(int32_t) +
                                                      sizeof(uint32_t) +
                                                      sizeof(TestCluster));
    }
    SECTION("incomplete cluster record") {
        std::filesystem::resize_file(
            file.path(), std::filesystem::file_size(file.path()) - 1);
    }

    ClusterFile<TestCluster> reader(file.path());
    if (use_roi) {
        reader.set_roi(aare::ROI{0, 10, 0, 10});
    }
    CHECK_THROWS_AS(reader.read_clusters(10), std::runtime_error);
}

TEST_CASE("ClusterFile chunk reads reject truncated leftover clusters",
          "[ClusterFile]") {
    const auto use_roi = GENERATE(false, true);
    const auto requested = GENERATE(1, 10);
    CAPTURE(use_roi, requested);
    TemporaryClusterFile file;
    auto expected = make_test_frame(42, 0.0);
    {
        ClusterFile<TestCluster> writer(file.path(), 1000, "w");
        writer.write_frame(expected);
    }
    std::filesystem::resize_file(file.path(),
                                 std::filesystem::file_size(file.path()) - 1);

    ClusterFile<TestCluster> reader(file.path());
    if (use_roi) {
        reader.set_roi(aare::ROI{0, 10, 0, 10});
    }
    auto first = reader.read_clusters(1);
    REQUIRE(first.size() == 1);
    CHECK(first[0].data == expected[0].data);
    CHECK_THROWS_AS(reader.read_clusters(requested), std::runtime_error);
}

TEST_CASE("ClusterFile chunk reads preserve valid frame traversal",
          "[ClusterFile]") {
    const auto use_roi = GENERATE(false, true);
    CAPTURE(use_roi);
    TemporaryClusterFile file;
    auto first = make_test_frame(42, 0.0);
    auto second = make_test_frame(43, 100.0);
    ClusterVector<TestCluster> empty(0);
    {
        ClusterFile<TestCluster> writer(file.path(), 1000, "w");
        writer.write_frame(empty);
        writer.write_frame(first);
        writer.write_frame(empty);
        writer.write_frame(second);
        writer.write_frame(empty);
    }

    ClusterFile<TestCluster> reader(file.path());
    if (use_roi) {
        reader.set_roi(aare::ROI{0, 10, 0, 10});
    }
    CHECK(reader.read_clusters(0).empty());
    CHECK(reader.tell() == 0);

    auto initial = reader.read_clusters(1);
    REQUIRE(initial.size() == 1);
    CHECK(initial[0].data == first[0].data);
    const auto position = reader.tell();
    CHECK(reader.read_clusters(0).empty());
    CHECK(reader.tell() == position);
    CHECK_THROWS_AS(reader.read_frame(), std::runtime_error);

    auto crossing = reader.read_clusters(2);
    REQUIRE(crossing.size() == 2);
    CHECK(crossing[0].data == first[1].data);
    CHECK(crossing[1].data == second[0].data);

    auto final = reader.read_clusters(10);
    REQUIRE(final.size() == 1);
    CHECK(final[0].data == second[1].data);
    CHECK(static_cast<uintmax_t>(reader.tell()) ==
          std::filesystem::file_size(file.path()));
    CHECK(reader.read_clusters(10).empty());
    CHECK_FALSE(reader.read_frame());
}

TEST_CASE("ClusterFile frames preserve empty frames and reuse storage",
          "[ClusterFile]") {
    TemporaryClusterFile file;
    auto first = make_test_frame(-42, 0.0);
    auto second = make_test_frame(105, 100.0);
    {
        ClusterFile<TestCluster> writer(file.path(), 1000, "w");
        writer.write_frame(ClusterVector<TestCluster>(0, -43));
        writer.write_frame(first);
        writer.write_frame(ClusterVector<TestCluster>(0, 0));
        writer.write_frame(second);
    }

    ClusterFile<TestCluster> reader(file.path());
    auto frames = reader.frames();
    CHECK(reader.tell() == 0);
    const TestCluster *storage = nullptr;
    std::vector<int32_t> frame_numbers;
    for (auto &frame : frames) {
        frame_numbers.push_back(frame.frame_number());
        if (frame.frame_number() == -42) {
            check_frame(frame, first);
            storage = frame.data();
        } else if (frame.frame_number() == 105) {
            check_frame(frame, second);
            CHECK(frame.data() == storage);
        } else {
            CHECK(frame.empty());
        }
    }
    CHECK(frame_numbers == std::vector<int32_t>{-43, -42, 0, 105});
    CHECK(frames.begin() == frames.end());
}

TEST_CASE("ClusterFile chunks preserve cluster order across frame boundaries",
          "[ClusterFile]") {
    const auto chunk_size = GENERATE(1, 2, 3, 10);
    TemporaryClusterFile file;
    auto first = make_test_frame(42, 0.0);
    auto second = make_test_frame(43, 100.0);
    {
        ClusterFile<TestCluster> writer(file.path(), 1000, "w");
        writer.write_frame(ClusterVector<TestCluster>(0));
        writer.write_frame(first);
        writer.write_frame(ClusterVector<TestCluster>(0));
        writer.write_frame(second);
        writer.write_frame(ClusterVector<TestCluster>(0));
    }

    ClusterFile<TestCluster> reader(file.path(), chunk_size);
    auto chunks = reader.chunks();
    auto requested = static_cast<size_t>(chunk_size);
    SECTION("constructor size") {}
    SECTION("explicit size overrides the constructor") {
        chunks = reader.chunks(3);
        requested = 3;
        CHECK(reader.chunk_size() == static_cast<size_t>(chunk_size));
    }
    CHECK(reader.tell() == 0);
    size_t count = 0;
    for (auto &chunk : chunks) {
        REQUIRE(chunk.size() == std::min(requested, 4 - count));
        for (const auto &cluster : chunk) {
            REQUIRE(count < 4);
            const auto &expected = count < 2 ? first[count] : second[count - 2];
            CHECK(cluster.x == expected.x);
            CHECK(cluster.y == expected.y);
            CHECK(cluster.data == expected.data);
            ++count;
        }
    }
    CHECK(count == 4);
    CHECK(chunks.begin() == chunks.end());
}

TEST_CASE("ClusterFile iterator results can be moved out without copying",
          "[ClusterFile]") {
    TemporaryClusterFile file;
    auto first = make_test_frame(42, 0.0);
    auto second = make_test_frame(43, 100.0);
    {
        ClusterFile<TestCluster> writer(file.path(), 1000, "w");
        writer.write_frame(first);
        writer.write_frame(second);
    }
    ClusterFile<TestCluster> reader(file.path(), 2);
    auto check_moves = [&](auto range) {
        auto it = range.begin();
        REQUIRE(it != range.end());
        CHECK(range.end() != it);
        const auto storage = it->data();
        auto retained = std::move(*it);
        CHECK(retained.data() == storage);
        ++it;
        REQUIRE(it != range.end());
        check_frame(*it, second);
        check_frame(retained, first);
        it++;
        CHECK(it == range.end());
        CHECK(range.end() == it);
        ++it;
        CHECK(it == range.end());
        reader.close();
        check_frame(retained, first);
    };
    SECTION("frames") { check_moves(reader.frames()); }
    SECTION("chunks") { check_moves(reader.chunks()); }
}

TEST_CASE("ClusterFile iteration preserves filtering and gain correction",
          "[ClusterFile]") {
    const auto use_roi = GENERATE(false, true);
    const auto use_noise = GENERATE(false, true);
    TemporaryClusterFile file;
    {
        ClusterFile<TestCluster> writer(file.path(), 1000, "w");
        writer.write_frame(make_test_frame(42, 0.0));
        writer.write_frame(make_test_frame(43, 100.0));
    }
    ClusterFile<TestCluster> reader(file.path(), 3);
    ClusterFile<TestCluster> reference(file.path());
    aare::NDArray<double, 2> gain({12, 12}, 2.0);
    aare::NDArray<int32_t, 2> noise({12, 12}, 5);
    for (auto *file_reader : {&reader, &reference}) {
        file_reader->set_gain_map(gain.view());
        if (use_roi) {
            file_reader->set_roi(aare::ROI{0, 6, 0, 12});
        }
        if (use_noise) {
            file_reader->set_noise_map(noise.view());
        }
    }
    SECTION("frames include fully rejected frames") {
        size_t count = 0;
        for (auto &frame : reader.frames()) {
            auto expected = reference.read_frame();
            REQUIRE(expected);
            check_frame(frame, *expected);
            ++count;
        }
        CHECK(count == 2);
        CHECK_FALSE(reference.read_frame());
    }
    SECTION("chunks count selected clusters") {
        for (auto &chunk : reader.chunks()) {
            auto expected = reference.read_clusters(3);
            check_frame(chunk, expected);
        }
        CHECK(reference.read_clusters(3).empty());
    }
}

TEST_CASE("ClusterFile iteration resumes after an early exit",
          "[ClusterFile]") {
    TemporaryClusterFile file;
    auto first = make_test_frame(42, 0.0);
    auto second = make_test_frame(43, 100.0);
    {
        ClusterFile<TestCluster> writer(file.path(), 1000, "w");
        writer.write_frame(first);
        writer.write_frame(second);
    }
    ClusterFile<TestCluster> reader(file.path());
    SECTION("breaking a frame loop does not consume the following frame") {
        for (auto &frame : reader.frames()) {
            check_frame(frame, first);
            break;
        }
        auto next = reader.read_frame();
        REQUIRE(next);
        check_frame(*next, second);
    }
    SECTION("partial chunks must be completed before frame iteration") {
        for (auto &chunk : reader.chunks(1)) {
            REQUIRE(chunk.size() == 1);
            CHECK(chunk[0].data == first[0].data);
            break;
        }
        CHECK_THROWS_AS(reader.frames().begin(), std::runtime_error);
        auto remainder = reader.read_clusters(1);
        REQUIRE(remainder.size() == 1);
        CHECK(remainder[0].data == first[1].data);
        auto it = reader.frames().begin();
        REQUIRE(it != reader.frames().end());
        check_frame(*it, second);
    }
}

TEST_CASE("ClusterFile iterators distinguish EOF from read errors",
          "[ClusterFile]") {
    TemporaryClusterFile file;
    {
        ClusterFile<TestCluster> writer(file.path(), 1000, "w");
        writer.write_frame(make_test_frame(42, 0.0));
    }
    auto check_error = [&] {
        ClusterFile<TestCluster> frame_reader(file.path());
        ClusterFile<TestCluster> chunk_reader(file.path());
        CHECK_THROWS_AS(frame_reader.frames().begin(), std::runtime_error);
        CHECK_THROWS_AS(chunk_reader.chunks().begin(), std::runtime_error);
    };
    SECTION("empty file") {
        std::filesystem::resize_file(file.path(), 0);
        ClusterFile<TestCluster> reader(file.path());
        CHECK(reader.frames().begin() == reader.frames().end());
        CHECK(reader.chunks().begin() == reader.chunks().end());
    }
    SECTION("partial header") {
        std::filesystem::resize_file(file.path(), 7);
        check_error();
    }
    SECTION("partial record") {
        std::filesystem::resize_file(
            file.path(), std::filesystem::file_size(file.path()) - 1);
        check_error();
    }
    SECTION("partial record encountered on increment") {
        std::filesystem::resize_file(
            file.path(), std::filesystem::file_size(file.path()) - 1);
        ClusterFile<TestCluster> reader(file.path());
        auto it = reader.chunks(1).begin();
        REQUIRE(it != reader.chunks(1).end());
        CHECK_THROWS_AS(++it, std::runtime_error);
    }
    SECTION("closed before first read") {
        ClusterFile<TestCluster> reader(file.path());
        auto frames = reader.frames();
        auto chunks = reader.chunks();
        reader.close();
        CHECK_THROWS_AS(frames.begin(), std::runtime_error);
        CHECK_THROWS_AS(chunks.begin(), std::runtime_error);
    }
    SECTION("closed during traversal") {
        ClusterFile<TestCluster> reader(file.path());
        auto it = reader.frames().begin();
        reader.close();
        CHECK_THROWS_AS(++it, std::runtime_error);
    }
    SECTION("write and append modes") {
        const auto mode = GENERATE("w", "a");
        ClusterFile<TestCluster> writer(file.path(), 1000, mode);
        CHECK_THROWS_AS(writer.frames().begin(), std::runtime_error);
        CHECK_THROWS_AS(writer.chunks().begin(), std::runtime_error);
    }
    SECTION("zero chunk sizes are rejected without reading") {
        ClusterFile<TestCluster> reader(file.path(), 0);
        CHECK_THROWS_AS(reader.chunks(), std::invalid_argument);
        CHECK_THROWS_AS(reader.chunks(0), std::invalid_argument);
        CHECK(reader.tell() == 0);
        CHECK(reader.read_clusters(0).empty());
        CHECK(reader.frames().begin() != reader.frames().end());
    }
}

TEST_CASE("Read frame and modify cluster data", "[.with-data]") {
    auto fpath = test_data_path() / "clust" / "single_frame_97_clustrers.clust";
    REQUIRE(std::filesystem::exists(fpath));

    ClusterFile<Cluster<int32_t, 3, 3>> f(fpath);

    auto clusters = f.read_frame();
    REQUIRE(clusters);
    CHECK(clusters->size() == 97);
    CHECK(clusters->frame_number() == 135);

    int32_t expected_cluster_data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    clusters->push_back(
        Cluster<int32_t, 3, 3>{0, 0, {0, 1, 2, 3, 4, 5, 6, 7, 8}});

    CHECK(clusters->size() == 98);
    CHECK((*clusters)[0].x == 1);
    CHECK((*clusters)[0].y == 200);

    CHECK(std::equal(std::begin((*clusters)[0].data),
                     std::end((*clusters)[0].data),
                     std::begin(expected_cluster_data)));
}
