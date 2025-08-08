#pragma once

#include <algorithm>
#include <map>
#include <unordered_map>
#include <vector>

#include "aare/NDArray.hpp"

const int MAX_CLUSTER_SIZE = 50;
namespace aare {

template <typename T> class VarClusterFinder {
  public:
    struct Hit {
        int16_t size{};
        int16_t row{};
        int16_t col{};
        uint16_t reserved{}; // for alignment
        T energy{};
        T max{};

        // std::vector<int16_t> rows{};
        // std::vector<int16_t> cols{};
        int16_t rows[MAX_CLUSTER_SIZE] = {0};
        int16_t cols[MAX_CLUSTER_SIZE] = {0};
        double enes[MAX_CLUSTER_SIZE] = {0};
    };

  private:
    const std::array<ssize_t, 2> shape_;
    NDView<T, 2> original_;
    NDArray<int, 2> labeled_;
    NDArray<int, 2> peripheral_labeled_;
    NDArray<bool, 2> binary_; // over threshold flag
    T threshold_;
    NDView<T, 2> noiseMap;
    bool use_noise_map = false;
    int peripheralThresholdFactor_ = 5;
    int current_label;
    const std::array<int, 4> di{
        {0, -1, -1, -1}}; // row ### 8-neighbour by scaning from left to right
    const std::array<int, 4> dj{
        {-1, -1, 0, 1}}; // col ### 8-neighbour by scaning from top to bottom
    const std::array<int, 8> di_{{0, 0, -1, 1, -1, 1, -1, 1}}; // row
    const std::array<int, 8> dj_{{-1, 1, 0, 0, 1, -1, -1, 1}}; // col
    std::map<int, int> child; // heirachy: key: child; val: parent
    std::unordered_map<int, Hit> h_size;
    std::vector<Hit> hits;
    // std::vector<std::vector<int16_t>> row
    int check_neighbours(int i, int j);

  public:
    VarClusterFinder(Shape<2> shape, T threshold)
        : shape_(shape), labeled_(shape, 0), peripheral_labeled_(shape, 0),
          binary_(shape), threshold_(threshold) {
        hits.reserve(2000);
    }

    NDArray<int, 2> labeled() { return labeled_; }

    void set_noiseMap(NDView<T, 2> noise_map) {
        noiseMap = noise_map;
        use_noise_map = true;
    }
    void set_peripheralThresholdFactor(int factor) {
        peripheralThresholdFactor_ = factor;
    }
    void find_clusters(NDView<T, 2> img);
    void find_clusters_X(NDView<T, 2> img);
    void rec_FillHit(int clusterIndex, int i, int j);
    void single_pass(NDView<T, 2> img);
    void first_pass();
    void second_pass();
    void store_clusters();

    std::vector<Hit> steal_hits() {
        std::vector<Hit> tmp;
        std::swap(tmp, hits);
        return tmp;
    };
    void clear_hits() { hits.clear(); };

    void print_connections() {
        fmt::print("Connections:\n");
        for (auto it = child.begin(); it != child.end(); ++it) {
            fmt::print("{} -> {}\n", it->first, it->second);
        }
    }
    size_t total_clusters() const {
        // TODO! fix for stealing
        return hits.size();
    }

  private:
    void add_link(int from, int to) {
        // we want to add key from -> value to
        // fmt::print("add_link({},{})\n", from, to);
        auto it = child.find(from);
        if (it == child.end()) {
            child[from] = to;
        } else {
            // found need to disambiguate
            if (it->second == to)
                return;
            else {
                if (it->second > to) {
                    // child[from] = to;
                    auto old = it->second;
                    it->second = to;
                    add_link(old, to);
                } else {
                    // found value is smaller than what we want to link
                    add_link(to, it->second);
                }
            }
        }
    }
};
template <typename T> int VarClusterFinder<T>::check_neighbours(int i, int j) {
    std::vector<int> neighbour_labels;

    for (int k = 0; k < 4; ++k) {
        const auto row = i + di[k];
        const auto col = j + dj[k];
        if (row >= 0 && col >= 0 && row < shape_[0] && col < shape_[1]) {
            auto tmp = labeled_.value(i + di[k], j + dj[k]);
            if (tmp != 0)
                neighbour_labels.push_back(tmp);
        }
    }

    if (neighbour_labels.size() == 0) {
        return 0;
    } else {

        // need to sort and add to union field
        std::sort(neighbour_labels.rbegin(), neighbour_labels.rend());
        auto first = neighbour_labels.begin();
        auto last = std::unique(first, neighbour_labels.end());
        if (last - first == 1)
            return *neighbour_labels.begin();

        for (auto current = first; current != last - 1; ++current) {
            auto next = current + 1;
            add_link(*current, *next);
        }
        return neighbour_labels.back(); // already sorted
    }
}

template <typename T>
void VarClusterFinder<T>::find_clusters(NDView<T, 2> img) {
    original_ = img;
    labeled_ = 0;
    peripheral_labeled_ = 0;
    current_label = 0;
    child.clear();
    first_pass();
    // print_connections();
    second_pass();
    store_clusters();
}

template <typename T>
void VarClusterFinder<T>::find_clusters_X(NDView<T, 2> img) {
    original_ = img;
    int clusterIndex = 0;
    for (int i = 0; i < shape_[0]; ++i) {
        for (int j = 0; j < shape_[1]; ++j) {
            if (use_noise_map)
                threshold_ = 5 * noiseMap(i, j);
            if (original_(i, j) > threshold_) {
                // printf("========== Cluster index: %d\n", clusterIndex);
                rec_FillHit(clusterIndex, i, j);
                clusterIndex++;
            }
        }
    }
    for (const auto &h : h_size)
        hits.push_back(h.second);
    h_size.clear();
}

template <typename T>
void VarClusterFinder<T>::rec_FillHit(int clusterIndex, int i, int j) {
    // printf("original_(%d, %d)=%f\n", i, j, original_(i,j));
    // printf("h_size[%d].size=%d\n", clusterIndex, h_size[clusterIndex].size);
    if (h_size[clusterIndex].size < MAX_CLUSTER_SIZE) {
        h_size[clusterIndex].rows[h_size[clusterIndex].size] = i;
        h_size[clusterIndex].cols[h_size[clusterIndex].size] = j;
        h_size[clusterIndex].enes[h_size[clusterIndex].size] = original_(i, j);
    }
    h_size[clusterIndex].size += 1;
    h_size[clusterIndex].energy += original_(i, j);
    if (h_size[clusterIndex].max < original_(i, j)) {
        h_size[clusterIndex].row = i;
        h_size[clusterIndex].col = j;
        h_size[clusterIndex].max = original_(i, j);
    }
    original_(i, j) = 0;

    for (int k = 0; k < 8; ++k) { // 8 for 8-neighbour
        const auto row = i + di_[k];
        const auto col = j + dj_[k];
        if (row >= 0 && col >= 0 && row < shape_[0] && col < shape_[1]) {
            if (use_noise_map)
                threshold_ = peripheralThresholdFactor_ * noiseMap(row, col);
            if (original_(row, col) > threshold_) {
                rec_FillHit(clusterIndex, row, col);
            } else {
                // if (h_size[clusterIndex].size < MAX_CLUSTER_SIZE){
                //     h_size[clusterIndex].size += 1;
                //     h_size[clusterIndex].rows[h_size[clusterIndex].size] =
                //     row; h_size[clusterIndex].cols[h_size[clusterIndex].size]
                //     = col;
                //     h_size[clusterIndex].enes[h_size[clusterIndex].size] =
                //     original_(row, col);
                // }// ? weather to include peripheral pixels
                original_(row, col) =
                    0; // remove peripheral pixels, to avoid potential influence
                       // for pedestal updating
            }
        }
    }
}

template <typename T> void VarClusterFinder<T>::single_pass(NDView<T, 2> img) {
    original_ = img;
    labeled_ = 0;
    current_label = 0;
    child.clear();
    first_pass();
    // print_connections();
    // second_pass();
    // store_clusters();
}

template <typename T> void VarClusterFinder<T>::first_pass() {

    for (ssize_t i = 0; i < original_.size(); ++i) {
        if (use_noise_map)
            threshold_ = 5 * noiseMap[i];
        binary_[i] = (original_[i] > threshold_);
    }

    for (int i = 0; i < shape_[0]; ++i) {
        for (int j = 0; j < shape_[1]; ++j) {

            // do we have something to process?
            if (binary_(i, j)) {
                auto tmp = check_neighbours(i, j);
                if (tmp != 0) {
                    labeled_(i, j) = tmp;
                } else {
                    labeled_(i, j) = ++current_label;
                }
            }
        }
    }
}

template <typename T> void VarClusterFinder<T>::second_pass() {

    for (ssize_t i = 0; i != labeled_.size(); ++i) {
        auto cl = labeled_(i);
        if (cl != 0) {
            auto it = child.find(cl);
            while (it != child.end()) {
                cl = it->second;
                it = child.find(cl);
                // do this once before doing the second pass?
                // all values point to the final one...
            }
            labeled_(i) = cl;
        }
    }
}

template <typename T> void VarClusterFinder<T>::store_clusters() {

    // Accumulate hit information in a map
    // Do we always have monotonic increasing
    // labels? Then vector?
    // here the translation is label -> Hit
    std::unordered_map<int, Hit> h_map;
    for (int i = 0; i < shape_[0]; ++i) {
        for (int j = 0; j < shape_[1]; ++j) {
            if (labeled_(i, j) != 0 || false
                // (i-1 >= 0 and labeled_(i-1, j) != 0) or // another circle of
                // peripheral pixels (j-1 >= 0 and labeled_(i, j-1) != 0) or
                // (i+1 < shape_[0] and labeled_(i+1, j) != 0) or
                // (j+1 < shape_[1] and labeled_(i, j+1) != 0)
            ) {
                Hit &record = h_map[labeled_(i, j)];
                if (record.size < MAX_CLUSTER_SIZE) {
                    record.rows[record.size] = i;
                    record.cols[record.size] = j;
                    record.enes[record.size] = original_(i, j);
                } else {
                    continue;
                }
                record.size += 1;
                record.energy += original_(i, j);

                if (record.max < original_(i, j)) {
                    record.row = i;
                    record.col = j;
                    record.max = original_(i, j);
                }
            }
        }
    }

    for (const auto &h : h_map)
        hits.push_back(h.second);
}

} // namespace aare
