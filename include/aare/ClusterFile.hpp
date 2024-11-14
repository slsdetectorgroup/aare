

#include "aare/defs.hpp"

#include <fstream>

namespace aare {

struct Cluster {
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

class ClusterFile {
    FILE *fp{};
    uint32_t m_num_left{};

  public:
    ClusterFile(const std::filesystem::path &fname);
    std::vector<Cluster> read_clusters(size_t n_clusters);
    std::vector<Cluster>
    read_cluster_with_cut(size_t n_clusters, double *noise_map, int nx, int ny);

    int analyze_data(int32_t *data, int32_t *t2, int32_t *t3, char *quad,
                 double *eta2x, double *eta2y, double *eta3x, double *eta3y);
    int analyze_cluster(Cluster cl, int32_t *t2, int32_t *t3, char *quad,
                    double *eta2x, double *eta2y, double *eta3x,
                    double *eta3y);

};

} // namespace aare