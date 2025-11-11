#include "aare/Cluster.hpp"
#include <benchmark/benchmark.h>

using namespace aare;

class ClustersForReduceFixture : public benchmark::Fixture {
  public:
    Cluster<int, 5, 5> cluster_5x5{};
    Cluster<int, 3, 3> cluster_3x3{};

  private:
    using benchmark::Fixture::SetUp;

    void SetUp([[maybe_unused]] const benchmark::State &state) override {
        int temp_data[25] = {1, 1, 1, 1, 1, 1, 1, 2, 1, 1,
                             1, 2, 3, 1, 2, 1, 1, 1, 1, 2};
        std::copy(std::begin(temp_data), std::end(temp_data),
                  std::begin(cluster_5x5.data));

        cluster_5x5.x = 5;
        cluster_5x5.y = 5;

        int temp_data2[9] = {1, 1, 1, 2, 3, 1, 2, 2, 1};
        std::copy(std::begin(temp_data2), std::end(temp_data2),
                  std::begin(cluster_3x3.data));

        cluster_3x3.x = 5;
        cluster_3x3.y = 5;
    }

    // void TearDown(::benchmark::State& state) {
    // }
};

template <typename T>
Cluster<T, 3, 3, uint16_t> reduce_to_3x3(const Cluster<T, 5, 5, uint16_t> &c) {
    Cluster<T, 3, 3, uint16_t> result;

    // Write out the sums in the hope that the compiler can optimize this
    std::array<T, 9> sum_3x3_subclusters;

    // Write out the sums in the hope that the compiler can optimize this
    sum_3x3_subclusters[0] = c.data[0] + c.data[1] + c.data[2] + c.data[5] +
                             c.data[6] + c.data[7] + c.data[10] + c.data[11] +
                             c.data[12];
    sum_3x3_subclusters[1] = c.data[1] + c.data[2] + c.data[3] + c.data[6] +
                             c.data[7] + c.data[8] + c.data[11] + c.data[12] +
                             c.data[13];
    sum_3x3_subclusters[2] = c.data[2] + c.data[3] + c.data[4] + c.data[7] +
                             c.data[8] + c.data[9] + c.data[12] + c.data[13] +
                             c.data[14];
    sum_3x3_subclusters[3] = c.data[5] + c.data[6] + c.data[7] + c.data[10] +
                             c.data[11] + c.data[12] + c.data[15] + c.data[16] +
                             c.data[17];
    sum_3x3_subclusters[4] = c.data[6] + c.data[7] + c.data[8] + c.data[11] +
                             c.data[12] + c.data[13] + c.data[16] + c.data[17] +
                             c.data[18];
    sum_3x3_subclusters[5] = c.data[7] + c.data[8] + c.data[9] + c.data[12] +
                             c.data[13] + c.data[14] + c.data[17] + c.data[18] +
                             c.data[19];
    sum_3x3_subclusters[6] = c.data[10] + c.data[11] + c.data[12] + c.data[15] +
                             c.data[16] + c.data[17] + c.data[20] + c.data[21] +
                             c.data[22];
    sum_3x3_subclusters[7] = c.data[11] + c.data[12] + c.data[13] + c.data[16] +
                             c.data[17] + c.data[18] + c.data[21] + c.data[22] +
                             c.data[23];
    sum_3x3_subclusters[8] = c.data[12] + c.data[13] + c.data[14] + c.data[17] +
                             c.data[18] + c.data[19] + c.data[22] + c.data[23] +
                             c.data[24];

    auto index = std::max_element(sum_3x3_subclusters.begin(),
                                  sum_3x3_subclusters.end()) -
                 sum_3x3_subclusters.begin();

    switch (index) {
    case 0:
        result.x = c.x - 1;
        result.y = c.y + 1;
        result.data = {c.data[0], c.data[1],  c.data[2],  c.data[5], c.data[6],
                       c.data[7], c.data[10], c.data[11], c.data[12]};
        break;
    case 1:
        result.x = c.x;
        result.y = c.y + 1;
        result.data = {c.data[1], c.data[2],  c.data[3],  c.data[6], c.data[7],
                       c.data[8], c.data[11], c.data[12], c.data[13]};
        break;
    case 2:
        result.x = c.x + 1;
        result.y = c.y + 1;
        result.data = {c.data[2], c.data[3],  c.data[4],  c.data[7], c.data[8],
                       c.data[9], c.data[12], c.data[13], c.data[14]};
        break;
    case 3:
        result.x = c.x - 1;
        result.y = c.y;
        result.data = {c.data[5],  c.data[6],  c.data[7],
                       c.data[10], c.data[11], c.data[12],
                       c.data[15], c.data[16], c.data[17]};
        break;
    case 4:
        result.x = c.x + 1;
        result.y = c.y;
        result.data = {c.data[6],  c.data[7],  c.data[8],
                       c.data[11], c.data[12], c.data[13],
                       c.data[16], c.data[17], c.data[18]};
        break;
    case 5:
        result.x = c.x + 1;
        result.y = c.y;
        result.data = {c.data[7],  c.data[8],  c.data[9],
                       c.data[12], c.data[13], c.data[14],
                       c.data[17], c.data[18], c.data[19]};
        break;
    case 6:
        result.x = c.x + 1;
        result.y = c.y - 1;
        result.data = {c.data[10], c.data[11], c.data[12],
                       c.data[15], c.data[16], c.data[17],
                       c.data[20], c.data[21], c.data[22]};
        break;
    case 7:
        result.x = c.x + 1;
        result.y = c.y - 1;
        result.data = {c.data[11], c.data[12], c.data[13],
                       c.data[16], c.data[17], c.data[18],
                       c.data[21], c.data[22], c.data[23]};
        break;
    case 8:
        result.x = c.x + 1;
        result.y = c.y - 1;
        result.data = {c.data[12], c.data[13], c.data[14],
                       c.data[17], c.data[18], c.data[19],
                       c.data[22], c.data[23], c.data[24]};
        break;
    }
    return result;
}

BENCHMARK_F(ClustersForReduceFixture, Reduce2x2)(benchmark::State &st) {
    for (auto _ : st) {
        // This code gets timed
        benchmark::DoNotOptimize(reduce_to_2x2<int, 3, 3, uint16_t>(
            cluster_3x3)); // make sure compiler evaluates the expression
    }
}

BENCHMARK_F(ClustersForReduceFixture, SpecificReduce2x2)(benchmark::State &st) {
    for (auto _ : st) {
        // This code gets timed
        benchmark::DoNotOptimize(reduce_to_2x2<int>(cluster_3x3));
    }
}

BENCHMARK_F(ClustersForReduceFixture, Reduce3x3)(benchmark::State &st) {
    for (auto _ : st) {
        // This code gets timed
        benchmark::DoNotOptimize(
            reduce_to_3x3<int, 5, 5, uint16_t>(cluster_5x5));
    }
}

BENCHMARK_F(ClustersForReduceFixture, SpecificReduce3x3)(benchmark::State &st) {
    for (auto _ : st) {
        // This code gets timed
        benchmark::DoNotOptimize(reduce_to_3x3<int>(cluster_5x5));
    }
}