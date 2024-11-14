#include "aare/ClusterFile.hpp"

namespace aare {

ClusterFile::ClusterFile(const std::filesystem::path &fname) {
    fp = fopen(fname.c_str(), "rb");
    if (!fp) {
        throw std::runtime_error("Could not open file: " + fname.string());
    }
}

std::vector<Cluster> ClusterFile::read_clusters(size_t n_clusters) {
    std::vector<Cluster> clusters(n_clusters);

    int32_t iframe = 0; // frame number needs to be 4 bytes!

    size_t nph_read = 0;

    //     uint32_t nn = *n_left;
    uint32_t nn = m_num_left;
    //     uint32_t nph = *n_left; // number of clusters in frame needs to be 4
    //     bytes!
    uint32_t nph = m_num_left;

    auto buf = reinterpret_cast<Cluster *>(clusters.data());
    // if there are photons left from previous frame read them first
    if (nph) {
        if (nph > n_clusters) {
            // if we have more photons left in the frame then photons to read we
            // read directly the requested number
            nn = n_clusters;
        } else {
            nn = nph;
        }
        nph_read += fread((void *)(buf + nph_read), sizeof(Cluster), nn, fp);
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

                nph_read +=
                    fread((void *)(buf + nph_read), sizeof(Cluster), nn, fp);
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

std::vector<Cluster> ClusterFile::read_cluster_with_cut(size_t n_clusters,
                                                        double *noise_map,
                                                        int nx, int ny) {

    std::vector<Cluster> clusters(n_clusters);
    // size_t read_clusters_with_cut(FILE *fp, size_t n_clusters, Cluster *buf,
    //                               uint32_t *n_left, double *noise_map, int
    //                               nx, int ny) {
    int iframe = 0;
    //     uint32_t nph = *n_left;
    uint32_t nph = m_num_left;
    //     uint32_t nn = *n_left;
    uint32_t nn = m_num_left;
    size_t nph_read = 0;

    int32_t t2max, tot1;
    int32_t tot3;
    // Cluster *ptr = buf;
    Cluster *ptr = clusters.data();
    int good = 1;
    double noise;
    // read photons left from previous frame
    if (noise_map)
        printf("Using noise map\n");

    if (nph) {
        if (nph > n_clusters) {
            // if we have more photons left in the frame then photons to
            // read we read directly the requested number
            nn = n_clusters;
        } else {
            nn = nph;
        }
        for (size_t iph = 0; iph < nn; iph++) {
            // read photons 1 by 1
            size_t n_read = fread((void *)(ptr), sizeof(Cluster), 1, fp);
            if (n_read != 1) {
                clusters.resize(nph_read);
                return clusters;
            }
            // TODO! error handling on read
            good = 1;
            if (noise_map) {
                if (ptr->x >= 0 && ptr->x < nx && ptr->y >= 0 && ptr->y < ny) {
                    tot1 = ptr->data[4];
                    analyze_cluster(*ptr, &t2max, &tot3, NULL, NULL, NULL, NULL,
                                    NULL);
                    noise = noise_map[ptr->y * nx + ptr->x];
                    if (tot1 > noise || t2max > 2 * noise || tot3 > 3 * noise) {
                        ;
                    } else {
                        good = 0;
                        printf("%d %d %f %d %d %d\n", ptr->x, ptr->y, noise,
                               tot1, t2max, tot3);
                    }
                } else {
                    printf("Bad pixel number %d %d\n", ptr->x, ptr->y);
                    good = 0;
                }
            }
            if (good) {
                ptr++;
                nph_read++;
            }
            (m_num_left)--;
            if (nph_read >= n_clusters)
                break;
        }
    }
        if (nph_read < n_clusters) {
    //         // keep on reading frames and photons until reaching n_clusters
            while (fread(&iframe, sizeof(iframe), 1, fp)) {
    //             // printf("%d\n",nph_read);

                if (fread(&nph, sizeof(nph), 1, fp)) {
    //                 // printf("** %d\n",nph);
                    m_num_left = nph;
                    for (size_t iph = 0; iph < nph; iph++) {
    //                     // read photons 1 by 1
                        size_t n_read =
                            fread((void *)(ptr), sizeof(Cluster), 1, fp);
                        if (n_read != 1) {
                            clusters.resize(nph_read);
                            return clusters;
                            // return nph_read;
                        }
                        good = 1;
                        if (noise_map) {
                            if (ptr->x >= 0 && ptr->x < nx && ptr->y >= 0 &&
                                ptr->y < ny) {
                                tot1 = ptr->data[4];
                                analyze_cluster(*ptr, &t2max, &tot3, NULL,
                                NULL,
                                                NULL, NULL, NULL);
                                // noise = noise_map[ptr->y * nx + ptr->x];
                                noise = noise_map[ptr->y + ny * ptr->x];
    			    if (tot1 > noise || t2max > 2 * noise ||
                                    tot3 > 3 * noise) {
                                    ;
                                } else
                                    good = 0;
                            } else {
                                printf("Bad pixel number %d %d\n", ptr->x,
                                ptr->y); good = 0;
                            }
                        }
                        if (good) {
                            ptr++;
    			nph_read++;
                        }
    		    (m_num_left)--;
                        if (nph_read >= n_clusters)
                            break;
                    }
                }
                if (nph_read >= n_clusters)
                    break;
            }
        }
        // printf("%d\n",nph_read);
        clusters.resize(nph_read);
        return clusters;

}

int ClusterFile::analyze_cluster(Cluster cl, int32_t *t2, int32_t *t3, char *quad,
                    double *eta2x, double *eta2y, double *eta3x,
                    double *eta3y) {

    return analyze_data(cl.data, t2, t3, quad, eta2x, eta2y, eta3x, eta3y);
}

int ClusterFile::analyze_data(int32_t *data, int32_t *t2, int32_t *t3, char *quad,
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
	//printf("*** %d %d %d %d -- %d\n",tot2[0],tot2[1],tot2[2],tot2[3],t2max);
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
                *eta2x = (double)(data[4]) / (data[3] + data[4]);
            if (eta2y && (data[1] + data[4]) != 0)
                *eta2y = (double)(data[4]) / (data[1] + data[4]);
            break;
        case cBottomRight:
            if (eta2x && (data[2] + data[5]) != 0)
                *eta2x = (double)(data[5]) / (data[4] + data[5]);
            if (eta2y && (data[1] + data[4]) != 0)
                *eta2y = (double)(data[4]) / (data[1] + data[4]);
            break;
        case cTopLeft:
            if (eta2x && (data[7] + data[4]) != 0)
                *eta2x = (double)(data[4]) / (data[3] + data[4]);
            if (eta2y && (data[7] + data[4]) != 0)
                *eta2y = (double)(data[7]) / (data[7] + data[4]);
            break;
        case cTopRight:
            if (eta2x && t2max != 0)
                *eta2x = (double)(data[5]) / (data[5] + data[4]);
            if (eta2y && t2max != 0)
                *eta2y = (double)(data[7]) / (data[7] + data[4]);
            break;
        default:;
        }
    }

    if (eta3x || eta3y) {
        if (eta3x && (data[3] + data[4] + data[5]) != 0)
            *eta3x = (double)(-data[3] + data[3 + 2]) /
                     (data[3] + data[4] + data[5]);
        if (eta3y && (data[1] + data[4] + data[7]) != 0)
            *eta3y = (double)(-data[1] + data[2 * 3 + 1]) /
                     (data[1] + data[4] + data[7]);
    }

    return ok;
}



} // namespace aare