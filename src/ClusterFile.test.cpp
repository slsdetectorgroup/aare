#include "aare/ClusterFile.hpp"
#include "test_config.hpp"

#include "aare/defs.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

using aare::Cluster;
using aare::ClusterFile;
using aare::ClusterVector;

TEST_CASE("Read one frame from a cluster file", "[.with-data]") {
    // We know that the frame has 97 clusters
    auto fpath = test_data_path() / "clust" / "single_frame_97_clustrers.clust";
    REQUIRE(std::filesystem::exists(fpath));

    ClusterFile<Cluster<int32_t, 3, 3>> f(fpath);
    auto clusters = f.read_frame();
    CHECK(clusters.size() == 97);
    CHECK(clusters.frame_number() == 135);
    CHECK(clusters[0].x == 1);
    CHECK(clusters[0].y == 200);
    int32_t expected_cluster_data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(std::equal(std::begin(clusters[0].data), std::end(clusters[0].data),
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
    REQUIRE(clusters.size() == 49);
    REQUIRE(clusters.frame_number() == 135);

    // Check that all clusters are within the ROI
    for (size_t i = 0; i < clusters.size(); i++) {
        auto c = clusters[i];
        REQUIRE(c.x >= roi.xmin);
        REQUIRE(c.x <= roi.xmax);
        REQUIRE(c.y >= roi.ymin);
        REQUIRE(c.y <= roi.ymax);
    }

    CHECK(clusters[0].x == 1);
    CHECK(clusters[0].y == 200);
    int32_t expected_cluster_data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(std::equal(std::begin(clusters[0].data), std::end(clusters[0].data),
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

TEST_CASE("Write cluster with potential padding", "[.with-data][.ClusterFile]") {

    using ClusterType = Cluster<double, 3, 3>;

    REQUIRE(std::filesystem::exists(test_data_path() / "clust"));

    auto fpath = test_data_path() / "clust" / "single_frame_2_clusters.clust";

    ClusterFile<ClusterType> file(fpath, 1000, "w");

    ClusterVector<ClusterType> clustervec(2);
    int16_t coordinate = 5;
    clustervec.push_back(ClusterType{
        coordinate, coordinate, {0., 0., 0., 0., 0., 0., 0., 0., 0.}});
    clustervec.push_back(ClusterType{
        coordinate, coordinate, {0., 0., 0., 0., 0., 0., 0., 0., 0.}});

    file.write_frame(clustervec);

    file.close();

    file.open("r");

    auto read_cluster_vector = file.read_frame();

    CHECK(read_cluster_vector.size() == 2);
    CHECK(read_cluster_vector.frame_number() == 0);

    CHECK(read_cluster_vector[0].x == clustervec[0].x);
    CHECK(read_cluster_vector[0].y == clustervec[0].y);
    CHECK(std::equal(
        clustervec[0].data.begin(), clustervec[0].data.end(),
        read_cluster_vector[0].data.begin(), [](double a, double b) {
            return std::abs(a - b) < std::numeric_limits<double>::epsilon();
        }));

    CHECK(read_cluster_vector[1].x == clustervec[1].x);
    CHECK(read_cluster_vector[1].y == clustervec[1].y);
    CHECK(std::equal(
        clustervec[1].data.begin(), clustervec[1].data.end(),
        read_cluster_vector[1].data.begin(), [](double a, double b) {
            return std::abs(a - b) < std::numeric_limits<double>::epsilon();
        }));
}

TEST_CASE("Read frame and modify cluster data", "[.with-data][.ClusterFile]") {
    auto fpath = test_data_path() / "clust" / "single_frame_97_clustrers.clust";
    REQUIRE(std::filesystem::exists(fpath));

    ClusterFile<Cluster<int32_t, 3, 3>> f(fpath);

    auto clusters = f.read_frame();
    CHECK(clusters.size() == 97);
    CHECK(clusters.frame_number() == 135);

    int32_t expected_cluster_data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    clusters.push_back(
        Cluster<int32_t, 3, 3>{0, 0, {0, 1, 2, 3, 4, 5, 6, 7, 8}});

    CHECK(clusters.size() == 98);
    CHECK(clusters[0].x == 1);
    CHECK(clusters[0].y == 200);

    CHECK(std::equal(std::begin(clusters[0].data), std::end(clusters[0].data),
                     std::begin(expected_cluster_data)));
}
