#pragma once

#include "aare/Cluster.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/NDArray.hpp"
#include "aare/defs.hpp"
#include <filesystem>
#include <fstream>

namespace aare {

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

// TODO: maybe template this!!!!!! why int32_t????
struct Eta2 {
    double x;
    double y;
    int c;
    int32_t sum;
};

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

// TODO: change to support any type of clusters, e.g. header line with
// clsuter_size_x, cluster_size_y,
/**
 * @brief Class to read and write cluster files
 * Expects data to be laid out as:
 *
 *
 *       int32_t frame_number
 *       uint32_t number_of_clusters
 *       int16_t x, int16_t y, int32_t data[9] x number_of_clusters
 *       int32_t frame_number
 *       uint32_t number_of_clusters
 *       etc.
 */
template <typename ClusterType,
          typename Enable = std::enable_if_t<is_cluster_v<ClusterType>, bool>>
class ClusterFile {
    FILE *fp{};
    uint32_t m_num_left{};
    size_t m_chunk_size{};
    const std::string m_mode;

  public:
    /**
     * @brief Construct a new Cluster File object
     * @param fname path to the file
     * @param chunk_size number of clusters to read at a time when iterating
     * over the file
     * @param mode mode to open the file in. "r" for reading, "w" for writing,
     * "a" for appending
     * @throws std::runtime_error if the file could not be opened
     */
    ClusterFile(const std::filesystem::path &fname, size_t chunk_size = 1000,
                const std::string &mode = "r");

    ~ClusterFile();

    /**
     * @brief Read n_clusters clusters from the file discarding frame numbers.
     * If EOF is reached the returned vector will have less than n_clusters
     * clusters
     */
    ClusterVector<ClusterType> read_clusters(size_t n_clusters);

    ClusterVector<ClusterType> read_clusters(size_t n_clusters, ROI roi);

    /**
     * @brief Read a single frame from the file and return the clusters. The
     * cluster vector will have the frame number set.
     * @throws std::runtime_error if the file is not opened for reading or the
     * file pointer not at the beginning of a frame
     */
    ClusterVector<ClusterType> read_frame();

    void write_frame(const ClusterVector<ClusterType> &clusters);

    // Need to be migrated to support NDArray and return a ClusterVector
    // std::vector<Cluster3x3>
    // read_cluster_with_cut(size_t n_clusters, double *noise_map, int nx, int
    // ny);

    /**
     * @brief Return the chunk size
     */
    size_t chunk_size() const { return m_chunk_size; }

    /**
     * @brief Close the file. If not closed the file will be closed in the
     * destructor
     */
    void close();
};

int analyze_data(int32_t *data, int32_t *t2, int32_t *t3, char *quad,
                 double *eta2x, double *eta2y, double *eta3x, double *eta3y);
int analyze_cluster(Cluster<int32_t, 3, 3> &cl, int32_t *t2, int32_t *t3,
                    char *quad, double *eta2x, double *eta2y, double *eta3x,
                    double *eta3y);

// template <typename ClusterType,
// typename = std::enable_if_t<is_cluster_v<ClusterType>>>
// NDArray<double, 2> calculate_eta2(ClusterVector<ClusterType> &clusters);

// TODO: do we need rquire clauses?
// template <typename T> Eta2 calculate_eta2(const Cluster<T, 3, 3> &cl);

// template <typename T> Eta2 calculate_eta2(const Cluster<T, 2, 2> &cl);

// template <typename ClusterType, std::enable_if_t<is_cluster_v<ClusterType>>>
// Eta2 calculate_eta2(const ClusterType &cl);

// template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
// typename CoordType>
// Eta2 calculate_eta2(
// const Cluster<T, ClusterSizeX, ClusterSizeY, CoordType> &cl);

template <typename ClusterType, typename Enable>
ClusterFile<ClusterType, Enable>::ClusterFile(
    const std::filesystem::path &fname, size_t chunk_size,
    const std::string &mode)
    : m_chunk_size(chunk_size), m_mode(mode) {

    if (mode == "r") {
        fp = fopen(fname.c_str(), "rb");
        if (!fp) {
            throw std::runtime_error("Could not open file for reading: " +
                                     fname.string());
        }
    } else if (mode == "w") {
        fp = fopen(fname.c_str(), "wb");
        if (!fp) {
            throw std::runtime_error("Could not open file for writing: " +
                                     fname.string());
        }
    } else if (mode == "a") {
        fp = fopen(fname.c_str(), "ab");
        if (!fp) {
            throw std::runtime_error("Could not open file for appending: " +
                                     fname.string());
        }
    } else {
        throw std::runtime_error("Unsupported mode: " + mode);
    }
}

template <typename ClusterType, typename Enable>
ClusterFile<ClusterType, Enable>::~ClusterFile() {
    close();
}

template <typename ClusterType, typename Enable>
void ClusterFile<ClusterType, Enable>::close() {
    if (fp) {
        fclose(fp);
        fp = nullptr;
    }
}

// TODO generally supported for all clsuter types
template <typename ClusterType, typename Enable>
void ClusterFile<ClusterType, Enable>::write_frame(
    const ClusterVector<ClusterType> &clusters) {
    if (m_mode != "w" && m_mode != "a") {
        throw std::runtime_error("File not opened for writing");
    }
    if (!(clusters.cluster_size_x() == 3) &&
        !(clusters.cluster_size_y() == 3)) {
        throw std::runtime_error("Only 3x3 clusters are supported");
    }
    int32_t frame_number = clusters.frame_number();
    fwrite(&frame_number, sizeof(frame_number), 1, fp);
    uint32_t n_clusters = clusters.size();
    fwrite(&n_clusters, sizeof(n_clusters), 1, fp);
    fwrite(clusters.data(), clusters.item_size(), clusters.size(), fp);
}

template <typename ClusterType, typename Enable>
ClusterVector<ClusterType>
ClusterFile<ClusterType, Enable>::read_clusters(size_t n_clusters) {
    if (m_mode != "r") {
        throw std::runtime_error("File not opened for reading");
    }

    ClusterVector<ClusterType> clusters(n_clusters);

    int32_t iframe = 0; // frame number needs to be 4 bytes!
    size_t nph_read = 0;
    uint32_t nn = m_num_left;
    uint32_t nph = m_num_left; // number of clusters in frame needs to be 4

    // auto buf = reinterpret_cast<Cluster3x3 *>(clusters.data());
    auto buf = clusters.data();
    // if there are photons left from previous frame read them first
    if (nph) {
        if (nph > n_clusters) {
            // if we have more photons left in the frame then photons to read we
            // read directly the requested number
            nn = n_clusters;
        } else {
            nn = nph;
        }
        nph_read += fread((buf + nph_read * clusters.item_size()),
                          clusters.item_size(), nn, fp);
        m_num_left = nph - nn; // write back the number of photons left
    }

    if (nph_read < n_clusters) {
        // keep on reading frames and photons until reaching n_clusters
        while (fread(&iframe, sizeof(iframe), 1, fp)) {
            // read number of clusters in frame
            if (fread(&nph, sizeof(nph), 1, fp)) {
                if (nph > (n_clusters - nph_read))
                    nn = n_clusters - nph_read;
                else
                    nn = nph;

                nph_read += fread((buf + nph_read * clusters.item_size()),
                                  clusters.item_size(), nn, fp);
                m_num_left = nph - nn;
            }
            if (nph_read >= n_clusters)
                break;
        }
    }

    // Resize the vector to the number of clusters.
    // No new allocation, only change bounds.
    clusters.resize(nph_read);
    return clusters;
}

template <typename ClusterType, typename Enable>
ClusterVector<ClusterType>
ClusterFile<ClusterType, Enable>::read_clusters(size_t n_clusters, ROI roi) {
    if (m_mode != "r") {
        throw std::runtime_error("File not opened for reading");
    }

    ClusterVector<ClusterType> clusters;
    clusters.reserve(n_clusters);

    int32_t iframe = 0; // frame number needs to be 4 bytes!
    size_t nph_read = 0;
    uint32_t nn = m_num_left;
    uint32_t nph = m_num_left; // number of clusters in frame needs to be 4

    // auto buf = reinterpret_cast<Cluster3x3 *>(clusters.data());
    // auto buf = clusters.data();

    ClusterType tmp; // this would break if the cluster size changes

    // if there are photons left from previous frame read them first
    if (nph) {
        if (nph > n_clusters) {
            // if we have more photons left in the frame then photons to read we
            // read directly the requested number
            nn = n_clusters;
        } else {
            nn = nph;
        }
        // Read one cluster, in the ROI push back
        //  nph_read += fread((buf + nph_read*clusters.item_size()),
        //                    clusters.item_size(), nn, fp);
        for (size_t i = 0; i < nn; i++) {
            fread(&tmp, sizeof(tmp), 1, fp);
            if (tmp.x >= roi.xmin && tmp.x <= roi.xmax && tmp.y >= roi.ymin &&
                tmp.y <= roi.ymax) {
                clusters.push_back(tmp.x, tmp.y,
                                   reinterpret_cast<std::byte *>(tmp.data));
                nph_read++;
            }
        }

        m_num_left = nph - nn; // write back the number of photons left
    }

    if (nph_read < n_clusters) {
        // keep on reading frames and photons until reaching n_clusters
        while (fread(&iframe, sizeof(iframe), 1, fp)) {
            // read number of clusters in frame
            if (fread(&nph, sizeof(nph), 1, fp)) {
                if (nph > (n_clusters - nph_read))
                    nn = n_clusters - nph_read;
                else
                    nn = nph;

                // nph_read += fread((buf + nph_read*clusters.item_size()),
                //                   clusters.item_size(), nn, fp);
                for (size_t i = 0; i < nn; i++) {
                    fread(&tmp, sizeof(tmp), 1, fp);
                    if (tmp.x >= roi.xmin && tmp.x <= roi.xmax &&
                        tmp.y >= roi.ymin && tmp.y <= roi.ymax) {
                        clusters.push_back(
                            tmp.x, tmp.y,
                            reinterpret_cast<std::byte *>(tmp.data));
                        nph_read++;
                    }
                }
                m_num_left = nph - nn;
            }
            if (nph_read >= n_clusters)
                break;
        }
    }

    // Resize the vector to the number of clusters.
    // No new allocation, only change bounds.
    clusters.resize(nph_read);
    return clusters;
}

template <typename ClusterType, typename Enable>
ClusterVector<ClusterType> ClusterFile<ClusterType, Enable>::read_frame() {
    if (m_mode != "r") {
        throw std::runtime_error("File not opened for reading");
    }
    if (m_num_left) {
        throw std::runtime_error(
            "There are still photons left in the last frame");
    }
    int32_t frame_number;
    if (fread(&frame_number, sizeof(frame_number), 1, fp) != 1) {
        throw std::runtime_error("Could not read frame number");
    }

    int32_t n_clusters; // Saved as 32bit integer in the cluster file
    if (fread(&n_clusters, sizeof(n_clusters), 1, fp) != 1) {
        throw std::runtime_error("Could not read number of clusters");
    }
    // std::vector<Cluster3x3> clusters(n_clusters);
    ClusterVector<ClusterType> clusters(n_clusters);
    clusters.set_frame_number(frame_number);

    if (fread(clusters.data(), clusters.item_size(), n_clusters, fp) !=
        static_cast<size_t>(n_clusters)) {
        throw std::runtime_error("Could not read clusters");
    }
    clusters.resize(n_clusters);
    return clusters;
}

template <typename ClusterType, std::enable_if_t<is_cluster_v<ClusterType>>>
NDArray<double, 2> calculate_eta2(const ClusterVector<ClusterType> &clusters) {
    // TOTO! make work with 2x2 clusters
    NDArray<double, 2> eta2({static_cast<int64_t>(clusters.size()), 2});

    for (size_t i = 0; i < clusters.size(); i++) {
        auto e = calculate_eta2(clusters.at(i));
        eta2(i, 0) = e.x;
        eta2(i, 1) = e.y;
    }

    return eta2;
}

/**
 * @brief Calculate the eta2 values for a generic sized cluster and return them
 * in a Eta2 struct containing etay, etax and the index of the respective 2x2
 * subcluster.
 */
template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType>
Eta2 calculate_eta2(
    const Cluster<T, ClusterSizeX, ClusterSizeY, CoordType> &cl) {
    Eta2 eta{};

    // TODO loads of overhead for a 2x2 clsuter maybe keep 2x2 calculation
    constexpr size_t num_2x2_subclusters =
        (ClusterSizeX - 1) * (ClusterSizeY - 1);
    std::array<T, num_2x2_subclusters> sum_2x2_subcluster;
    for (size_t i = 0; i < ClusterSizeY - 1; ++i) {
        for (size_t j = 0; j < ClusterSizeX - 1; ++j)
            sum_2x2_subcluster[i * (ClusterSizeX - 1) + j] =
                cl.data[i * ClusterSizeX + j] +
                cl.data[i * ClusterSizeX + j + 1] +
                cl.data[(i + 1) * ClusterSizeX + j] +
                cl.data[(i + 1) * ClusterSizeX + j + 1];
    }

    auto c =
        std::max_element(sum_2x2_subcluster.begin(), sum_2x2_subcluster.end()) -
        sum_2x2_subcluster.begin();

    eta.sum = sum_2x2_subcluster[c];

    size_t index_bottom_left_max_2x2_subcluster =
        (int(c / (ClusterSizeX - 1))) * ClusterSizeX + c % (ClusterSizeX - 1);

    if ((cl.data[index_bottom_left_max_2x2_subcluster] +
         cl.data[index_bottom_left_max_2x2_subcluster + 1]) != 0)
        eta.x = static_cast<double>(
                    cl.data[index_bottom_left_max_2x2_subcluster + 1]) /
                (cl.data[index_bottom_left_max_2x2_subcluster] +
                 cl.data[index_bottom_left_max_2x2_subcluster + 1]);

    if ((cl.data[index_bottom_left_max_2x2_subcluster] +
         cl.data[index_bottom_left_max_2x2_subcluster + ClusterSizeX]) != 0)
        eta.y =
            static_cast<double>(
                cl.data[index_bottom_left_max_2x2_subcluster + ClusterSizeX]) /
            (cl.data[index_bottom_left_max_2x2_subcluster] +
             cl.data[index_bottom_left_max_2x2_subcluster + ClusterSizeX]);

    eta.c = c; // TODO only supported for 2x2 and 3x3 clusters -> at least no
               // underyling enum class
    return eta;
}

/**
 * @brief Calculate the eta2 values for a 3x3 cluster and return them in a Eta2
 * struct containing etay, etax and the corner of the cluster.
 */
/*
template <typename T> Eta2 calculate_eta2(const Cluster<T, 3, 3> &cl) {
    Eta2 eta{};

    std::array<T, 4> tot2;
    tot2[0] = cl.data[0] + cl.data[1] + cl.data[3] + cl.data[4];
    tot2[1] = cl.data[1] + cl.data[2] + cl.data[4] + cl.data[5];
    tot2[2] = cl.data[3] + cl.data[4] + cl.data[6] + cl.data[7];
    tot2[3] = cl.data[4] + cl.data[5] + cl.data[7] + cl.data[8];

    auto c = std::max_element(tot2.begin(), tot2.end()) - tot2.begin();
    eta.sum = tot2[c];
    switch (c) {
    case cBottomLeft:
        if ((cl.data[3] + cl.data[4]) != 0)
            eta.x = static_cast<double>(cl.data[4]) / (cl.data[3] + cl.data[4]);
        if ((cl.data[1] + cl.data[4]) != 0)
            eta.y = static_cast<double>(cl.data[4]) / (cl.data[1] + cl.data[4]);
        eta.c = cBottomLeft;
        break;
    case cBottomRight:
        if ((cl.data[2] + cl.data[5]) != 0)
            eta.x = static_cast<double>(cl.data[5]) / (cl.data[4] + cl.data[5]);
        if ((cl.data[1] + cl.data[4]) != 0)
            eta.y = static_cast<double>(cl.data[4]) / (cl.data[1] + cl.data[4]);
        eta.c = cBottomRight;
        break;
    case cTopLeft:
        if ((cl.data[7] + cl.data[4]) != 0)
            eta.x = static_cast<double>(cl.data[4]) / (cl.data[3] + cl.data[4]);
        if ((cl.data[7] + cl.data[4]) != 0)
            eta.y = static_cast<double>(cl.data[7]) / (cl.data[7] + cl.data[4]);
        eta.c = cTopLeft;
        break;
    case cTopRight:
        if ((cl.data[5] + cl.data[4]) != 0)
            eta.x = static_cast<double>(cl.data[5]) / (cl.data[5] + cl.data[4]);
        if ((cl.data[7] + cl.data[4]) != 0)
            eta.y = static_cast<double>(cl.data[7]) / (cl.data[7] + cl.data[4]);
        eta.c = cTopRight;
        break;
    }
    return eta;
}
*/

/*
template <typename T> Eta2 calculate_eta2(const Cluster<T, 2, 2> &cl) {
    Eta2 eta{};

    eta.x = static_cast<double>(cl.data[1]) / (cl.data[0] + cl.data[1]);
    eta.y = static_cast<double>(cl.data[2]) / (cl.data[0] + cl.data[2]);
    eta.sum = cl.data[0] + cl.data[1] + cl.data[2] + cl.data[3];
    eta.c = cBottomLeft; // TODO! This is not correct, but need to put something
    return eta;
}
*/

// TODO complicated API simplify?
int analyze_cluster(Cluster<int32_t, 3, 3> &cl, int32_t *t2, int32_t *t3,
                    char *quad, double *eta2x, double *eta2y, double *eta3x,
                    double *eta3y) {

    return analyze_data(cl.data, t2, t3, quad, eta2x, eta2y, eta3x, eta3y);
}

int analyze_data(int32_t *data, int32_t *t2, int32_t *t3, char *quad,
                 double *eta2x, double *eta2y, double *eta3x, double *eta3y) {

    int ok = 1;

    int32_t tot2[4];
    int32_t t2max = 0;
    char c = 0;
    int32_t val, tot3;

    tot3 = 0;
    for (int i = 0; i < 4; i++)
        tot2[i] = 0;

    for (int ix = 0; ix < 3; ix++) {
        for (int iy = 0; iy < 3; iy++) {
            val = data[iy * 3 + ix];
            //	printf ("%d ",data[iy * 3 + ix]);
            tot3 += val;
            if (ix <= 1 && iy <= 1)
                tot2[cBottomLeft] += val;
            if (ix >= 1 && iy <= 1)
                tot2[cBottomRight] += val;
            if (ix <= 1 && iy >= 1)
                tot2[cTopLeft] += val;
            if (ix >= 1 && iy >= 1)
                tot2[cTopRight] += val;
        }
        //	printf ("\n");
    }
    // printf ("\n");

    if (t2 || quad) {

        t2max = tot2[0];
        c = cBottomLeft;
        for (int i = 1; i < 4; i++) {
            if (tot2[i] > t2max) {
                t2max = tot2[i];
                c = i;
            }
        }
        // printf("*** %d %d %d %d --
        // %d\n",tot2[0],tot2[1],tot2[2],tot2[3],t2max);
        if (quad)
            *quad = c;
        if (t2)
            *t2 = t2max;
    }

    if (t3)
        *t3 = tot3;

    if (eta2x || eta2y) {
        if (eta2x)
            *eta2x = 0;
        if (eta2y)
            *eta2y = 0;
        switch (c) {
        case cBottomLeft:
            if (eta2x && (data[3] + data[4]) != 0)
                *eta2x = static_cast<double>(data[4]) / (data[3] + data[4]);
            if (eta2y && (data[1] + data[4]) != 0)
                *eta2y = static_cast<double>(data[4]) / (data[1] + data[4]);
            break;
        case cBottomRight:
            if (eta2x && (data[2] + data[5]) != 0)
                *eta2x = static_cast<double>(data[5]) / (data[4] + data[5]);
            if (eta2y && (data[1] + data[4]) != 0)
                *eta2y = static_cast<double>(data[4]) / (data[1] + data[4]);
            break;
        case cTopLeft:
            if (eta2x && (data[7] + data[4]) != 0)
                *eta2x = static_cast<double>(data[4]) / (data[3] + data[4]);
            if (eta2y && (data[7] + data[4]) != 0)
                *eta2y = static_cast<double>(data[7]) / (data[7] + data[4]);
            break;
        case cTopRight:
            if (eta2x && t2max != 0)
                *eta2x = static_cast<double>(data[5]) / (data[5] + data[4]);
            if (eta2y && t2max != 0)
                *eta2y = static_cast<double>(data[7]) / (data[7] + data[4]);
            break;
        default:;
        }
    }

    if (eta3x || eta3y) {
        if (eta3x && (data[3] + data[4] + data[5]) != 0)
            *eta3x = static_cast<double>(-data[3] + data[3 + 2]) /
                     (data[3] + data[4] + data[5]);
        if (eta3y && (data[1] + data[4] + data[7]) != 0)
            *eta3y = static_cast<double>(-data[1] + data[2 * 3 + 1]) /
                     (data[1] + data[4] + data[7]);
    }

    return ok;
}

} // namespace aare
