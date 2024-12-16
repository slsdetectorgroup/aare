#pragma once

#include "aare/ClusterVector.hpp"
#include "aare/NDArray.hpp"
#include "aare/defs.hpp"
#include <filesystem>
#include <fstream>

namespace aare {

struct Cluster3x3 {
    int16_t x;
    int16_t y;
    int32_t data[9];
};

typedef enum {
    cBottomLeft = 0,
    cBottomRight = 1,
    cTopLeft = 2,
    cTopRight = 3
} corner;

typedef enum {
    pBottomLeft = 0,
    pBottom = 1,
    pBottomRight = 2,
    pLeft = 3,
    pCenter = 4,
    pRight = 5,
    pTopLeft = 6,
    pTop = 7,
    pTopRight = 8
} pixel;

struct ClusterAnalysis {
    uint32_t c;
    int32_t tot;
    double etax;
    double etay;
};

/*
Binary cluster file. Expects data to be layed out as:
int32_t frame_number
uint32_t number_of_clusters
int16_t x, int16_t y, int32_t data[9] x number_of_clusters
int32_t frame_number
uint32_t number_of_clusters
....
*/
class ClusterFile {
    FILE *fp{};
    uint32_t m_num_left{};
    size_t m_chunk_size{};
    const std::string m_mode;

  public:
    ClusterFile(const std::filesystem::path &fname, size_t chunk_size = 1000,
                const std::string &mode = "r");
    ~ClusterFile();
    std::vector<Cluster3x3> read_clusters(size_t n_clusters);
    std::vector<Cluster3x3> read_frame(int32_t &out_fnum);
    void write_frame(int32_t frame_number,
                     const ClusterVector<int32_t> &clusters);
    std::vector<Cluster3x3>
    read_cluster_with_cut(size_t n_clusters, double *noise_map, int nx, int ny);

    size_t chunk_size() const { return m_chunk_size; }
    void close();
};

int analyze_data(int32_t *data, int32_t *t2, int32_t *t3, char *quad,
                 double *eta2x, double *eta2y, double *eta3x, double *eta3y);
int analyze_cluster(Cluster3x3& cl, int32_t *t2, int32_t *t3, char *quad,
                    double *eta2x, double *eta2y, double *eta3x, double *eta3y);

NDArray<double, 2> calculate_eta2( ClusterVector<int>& clusters);
std::array<double,2> calculate_eta2( Cluster3x3& cl);

} // namespace aare
