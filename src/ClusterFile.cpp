#include "aare/ClusterFile.hpp"

#include <algorithm>

namespace aare {

ClusterFile::ClusterFile(const std::filesystem::path &fname, size_t chunk_size,
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

ClusterFile::~ClusterFile() { close(); }

void ClusterFile::close() {
    if (fp) {
        fclose(fp);
        fp = nullptr;
    }
}

void ClusterFile::write_frame(const ClusterVector<int32_t> &clusters) {
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

ClusterVector<int32_t> ClusterFile::read_clusters(size_t n_clusters) {
    if (m_mode != "r") {
        throw std::runtime_error("File not opened for reading");
    }
    
    ClusterVector<int32_t> clusters(3,3, n_clusters);

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
        nph_read += fread((buf + nph_read*clusters.item_size()),
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

                nph_read += fread((buf + nph_read*clusters.item_size()),
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

ClusterVector<int32_t> ClusterFile::read_clusters(size_t n_clusters, ROI roi) {
    if (m_mode != "r") {
        throw std::runtime_error("File not opened for reading");
    }
    
    ClusterVector<int32_t> clusters(3,3);
    clusters.reserve(n_clusters);

    int32_t iframe = 0; // frame number needs to be 4 bytes!
    size_t nph_read = 0;
    uint32_t nn = m_num_left;
    uint32_t nph = m_num_left; // number of clusters in frame needs to be 4

    // auto buf = reinterpret_cast<Cluster3x3 *>(clusters.data());
    // auto buf = clusters.data();

    Cluster3x3 tmp; //this would break if the cluster size changes

    // if there are photons left from previous frame read them first
    if (nph) {
        if (nph > n_clusters) {
            // if we have more photons left in the frame then photons to read we
            // read directly the requested number
            nn = n_clusters;
        } else {
            nn = nph;
        }
        //Read one cluster, in the ROI push back 
        // nph_read += fread((buf + nph_read*clusters.item_size()),
        //                   clusters.item_size(), nn, fp);
        for(size_t i = 0; i < nn; i++){
            fread(&tmp, sizeof(tmp), 1, fp);
            if(tmp.x >= roi.xmin && tmp.x <= roi.xmax && tmp.y >= roi.ymin && tmp.y <= roi.ymax){
                clusters.push_back(tmp.x, tmp.y, reinterpret_cast<std::byte*>(tmp.data));
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
                for(size_t i = 0; i < nn; i++){
                    fread(&tmp, sizeof(tmp), 1, fp);
                    if(tmp.x >= roi.xmin && tmp.x <= roi.xmax && tmp.y >= roi.ymin && tmp.y <= roi.ymax){
                        clusters.push_back(tmp.x, tmp.y, reinterpret_cast<std::byte*>(tmp.data));
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

ClusterVector<int32_t> ClusterFile::read_frame() {
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
    ClusterVector<int32_t> clusters(3, 3, n_clusters);
    clusters.set_frame_number(frame_number);

    if (fread(clusters.data(), clusters.item_size(), n_clusters, fp) !=
        static_cast<size_t>(n_clusters)) {
        throw std::runtime_error("Could not read clusters");
    }
    clusters.resize(n_clusters);
    return clusters;
}


// std::vector<Cluster3x3> ClusterFile::read_cluster_with_cut(size_t n_clusters,
//                                                            double *noise_map,
//                                                            int nx, int ny) {
//     if (m_mode != "r") {
//         throw std::runtime_error("File not opened for reading");
//     }
//     std::vector<Cluster3x3> clusters(n_clusters);
//     // size_t read_clusters_with_cut(FILE *fp, size_t n_clusters, Cluster *buf,
//     //                               uint32_t *n_left, double *noise_map, int
//     //                               nx, int ny) {
//     int iframe = 0;
//     //     uint32_t nph = *n_left;
//     uint32_t nph = m_num_left;
//     //     uint32_t nn = *n_left;
//     uint32_t nn = m_num_left;
//     size_t nph_read = 0;

//     int32_t t2max, tot1;
//     int32_t tot3;
//     // Cluster *ptr = buf;
//     Cluster3x3 *ptr = clusters.data();
//     int good = 1;
//     double noise;
//     // read photons left from previous frame
//     if (noise_map)
//         printf("Using noise map\n");

//     if (nph) {
//         if (nph > n_clusters) {
//             // if we have more photons left in the frame then photons to
//             // read we read directly the requested number
//             nn = n_clusters;
//         } else {
//             nn = nph;
//         }
//         for (size_t iph = 0; iph < nn; iph++) {
//             // read photons 1 by 1
//             size_t n_read =
//                 fread(reinterpret_cast<void *>(ptr), sizeof(Cluster3x3), 1, fp);
//             if (n_read != 1) {
//                 clusters.resize(nph_read);
//                 return clusters;
//             }
//             // TODO! error handling on read
//             good = 1;
//             if (noise_map) {
//                 if (ptr->x >= 0 && ptr->x < nx && ptr->y >= 0 && ptr->y < ny) {
//                     tot1 = ptr->data[4];
//                     analyze_cluster(*ptr, &t2max, &tot3, NULL, NULL, NULL, NULL,
//                                     NULL);
//                     noise = noise_map[ptr->y * nx + ptr->x];
//                     if (tot1 > noise || t2max > 2 * noise || tot3 > 3 * noise) {
//                         ;
//                     } else {
//                         good = 0;
//                         printf("%d %d %f %d %d %d\n", ptr->x, ptr->y, noise,
//                                tot1, t2max, tot3);
//                     }
//                 } else {
//                     printf("Bad pixel number %d %d\n", ptr->x, ptr->y);
//                     good = 0;
//                 }
//             }
//             if (good) {
//                 ptr++;
//                 nph_read++;
//             }
//             (m_num_left)--;
//             if (nph_read >= n_clusters)
//                 break;
//         }
//     }
//     if (nph_read < n_clusters) {
//         //         // keep on reading frames and photons until reaching
//         //         n_clusters
//         while (fread(&iframe, sizeof(iframe), 1, fp)) {
//             //             // printf("%d\n",nph_read);

//             if (fread(&nph, sizeof(nph), 1, fp)) {
//                 //                 // printf("** %d\n",nph);
//                 m_num_left = nph;
//                 for (size_t iph = 0; iph < nph; iph++) {
//                     //                     // read photons 1 by 1
//                     size_t n_read = fread(reinterpret_cast<void *>(ptr),
//                                           sizeof(Cluster3x3), 1, fp);
//                     if (n_read != 1) {
//                         clusters.resize(nph_read);
//                         return clusters;
//                         // return nph_read;
//                     }
//                     good = 1;
//                     if (noise_map) {
//                         if (ptr->x >= 0 && ptr->x < nx && ptr->y >= 0 &&
//                             ptr->y < ny) {
//                             tot1 = ptr->data[4];
//                             analyze_cluster(*ptr, &t2max, &tot3, NULL, NULL,
//                                             NULL, NULL, NULL);
//                             // noise = noise_map[ptr->y * nx + ptr->x];
//                             noise = noise_map[ptr->y + ny * ptr->x];
//                             if (tot1 > noise || t2max > 2 * noise ||
//                                 tot3 > 3 * noise) {
//                                 ;
//                             } else
//                                 good = 0;
//                         } else {
//                             printf("Bad pixel number %d %d\n", ptr->x, ptr->y);
//                             good = 0;
//                         }
//                     }
//                     if (good) {
//                         ptr++;
//                         nph_read++;
//                     }
//                     (m_num_left)--;
//                     if (nph_read >= n_clusters)
//                         break;
//                 }
//             }
//             if (nph_read >= n_clusters)
//                 break;
//         }
//     }
//     // printf("%d\n",nph_read);
//     clusters.resize(nph_read);
//     return clusters;
// }

NDArray<double, 2> calculate_eta2(ClusterVector<int> &clusters) {
    //TOTO! make work with 2x2 clusters
    NDArray<double, 2> eta2({static_cast<int64_t>(clusters.size()), 2});
    
    if (clusters.cluster_size_x() == 3 || clusters.cluster_size_y() == 3) {
        for (size_t i = 0; i < clusters.size(); i++) {
            auto e = calculate_eta2(clusters.at<Cluster3x3>(i));
            eta2(i, 0) = e.x;
            eta2(i, 1) = e.y;
        }
    }else if(clusters.cluster_size_x() == 2 || clusters.cluster_size_y() == 2){
        for (size_t i = 0; i < clusters.size(); i++) {
            auto e = calculate_eta2(clusters.at<Cluster2x2>(i));
            eta2(i, 0) = e.x;
            eta2(i, 1) = e.y;
        }
    }else{
        throw std::runtime_error("Only 3x3 and 2x2 clusters are supported");
    }
    
    return eta2;
}

/** 
 * @brief Calculate the eta2 values for a 3x3 cluster and return them in a Eta2 struct
 * containing etay, etax and the corner of the cluster. 
*/
Eta2 calculate_eta2(Cluster3x3 &cl) {
    Eta2 eta{};

    std::array<int32_t, 4> tot2;
    tot2[0] = cl.data[0] + cl.data[1] + cl.data[3] + cl.data[4];
    tot2[1] = cl.data[1] + cl.data[2] + cl.data[4] + cl.data[5];
    tot2[2] = cl.data[3] + cl.data[4] + cl.data[6] + cl.data[7];
    tot2[3] = cl.data[4] + cl.data[5] + cl.data[7] + cl.data[8];

    auto c = std::max_element(tot2.begin(), tot2.end()) - tot2.begin();
    eta.sum = tot2[c];
    switch (c) {
    case cBottomLeft:
        if ((cl.data[3] + cl.data[4]) != 0)
            eta.x =
                static_cast<double>(cl.data[4]) / (cl.data[3] + cl.data[4]);
        if ((cl.data[1] + cl.data[4]) != 0)
            eta.y =
                static_cast<double>(cl.data[4]) / (cl.data[1] + cl.data[4]);
        eta.c = cBottomLeft;
        break;
    case cBottomRight:
        if ((cl.data[2] + cl.data[5]) != 0)
            eta.x =
                static_cast<double>(cl.data[5]) / (cl.data[4] + cl.data[5]);
        if ((cl.data[1] + cl.data[4]) != 0)
            eta.y =
                static_cast<double>(cl.data[4]) / (cl.data[1] + cl.data[4]);
        eta.c = cBottomRight;
        break;
    case cTopLeft:
        if ((cl.data[7] + cl.data[4]) != 0)
            eta.x =
                static_cast<double>(cl.data[4]) / (cl.data[3] + cl.data[4]);
        if ((cl.data[7] + cl.data[4]) != 0)
            eta.y =
                static_cast<double>(cl.data[7]) / (cl.data[7] + cl.data[4]);
        eta.c = cTopLeft;
        break;
    case cTopRight:
        if ((cl.data[5] + cl.data[4]) != 0)
            eta.x =
                static_cast<double>(cl.data[5]) / (cl.data[5] + cl.data[4]);
        if ((cl.data[7] + cl.data[4]) != 0)
            eta.y =
                static_cast<double>(cl.data[7]) / (cl.data[7] + cl.data[4]);
        eta.c = cTopRight;
        break;
    // no default to allow compiler to warn about missing cases
    }
    return eta;
}


Eta2 calculate_eta2(Cluster2x2 &cl) {
    Eta2 eta{};
    if ((cl.data[0] + cl.data[1]) != 0)
        eta.x = static_cast<double>(cl.data[1]) / (cl.data[0] + cl.data[1]);
    if ((cl.data[0] + cl.data[2]) != 0)
        eta.y = static_cast<double>(cl.data[2]) / (cl.data[0] + cl.data[2]);
    eta.sum = cl.data[0] + cl.data[1] + cl.data[2]+ cl.data[3];
    eta.c = cBottomLeft; //TODO! This is not correct, but need to put something
    return eta;
}



int analyze_cluster(Cluster3x3 &cl, int32_t *t2, int32_t *t3, char *quad,
                    double *eta2x, double *eta2y, double *eta3x,
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