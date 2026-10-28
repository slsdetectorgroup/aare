# SPDX-License-Identifier: MPL-2.0
import gc

import numpy as np
import pytest

from aare import VarClusterFinder, VarClusterHits


def frame_with_cluster(shape=(8, 8), row=3, col=4):
    img = np.zeros(shape, dtype=np.float64)
    img[row - 1 : row + 2, col - 1 : col + 2] = 20.0
    img[row, col] = 100.0
    return img


def test_single_cluster_fixed_threshold():
    img = frame_with_cluster()
    cf = VarClusterFinder((8, 8), 10.0)
    cf.find_clusters(img)
    hits = cf.steal_hits()

    assert isinstance(hits, VarClusterHits)
    assert len(hits) == 1
    hit = hits.hits[0]
    assert hit["size"] == 9
    assert hit["offset"] == 0
    assert hit["row"] == 3
    assert hit["col"] == 4
    assert hit["max"] == 100.0
    assert hit["energy"] == pytest.approx(260.0)

    assert hits.rows.shape == (9,)
    assert hits.cols.shape == (9,)
    assert hits.values.shape == (9,)
    stored = {(int(r), int(c)) for r, c in zip(hits.rows, hits.cols)}
    assert stored == {(r, c) for r in range(2, 5) for c in range(3, 6)}
    assert hits.values.sum() == pytest.approx(260.0)
    # The seed comes first
    assert (hits.rows[0], hits.cols[0]) == (2, 3)


def test_hit_dtype_and_array_types():
    cf = VarClusterFinder((8, 8), 10.0)
    hits = cf.steal_hits()
    assert hits.hits.dtype.names == ("size", "row", "col", "energy", "max", "offset")
    assert hits.hits.dtype["size"] == np.int32
    assert hits.hits.dtype["row"] == np.int16
    assert hits.hits.dtype["energy"] == np.float64
    assert hits.hits.dtype["offset"] == np.int64
    assert hits.hits.dtype.itemsize == 32
    assert hits.rows.dtype == np.int16
    assert hits.cols.dtype == np.int16
    assert hits.values.dtype == np.float64


def test_pixels_of_each_hit():
    img = frame_with_cluster()
    img[7, 0] = 50.0
    cf = VarClusterFinder((8, 8), 10.0)
    cf.find_clusters(img)
    hits = cf.steal_hits()

    assert len(hits) == 2
    assert hits.hits["offset"].tolist() == [0, 9]
    rows, cols, values = hits.pixels(0)
    assert rows.shape == (9,)
    assert values.sum() == pytest.approx(260.0)
    rows, cols, values = hits.pixels(1)
    assert (rows.tolist(), cols.tolist(), values.tolist()) == ([7], [0], [50.0])
    rows, cols, values = hits.pixels(-1)
    assert rows.tolist() == [7]
    with pytest.raises(IndexError):
        hits.pixels(2)
    with pytest.raises(IndexError):
        hits.pixels(-3)

    # Vectorised per hit sums through the offsets
    energies = np.add.reduceat(hits.values, hits.hits["offset"])
    assert energies == pytest.approx(hits.hits["energy"])


def test_arrays_keep_the_hits_alive():
    cf = VarClusterFinder((8, 8), 10.0)
    cf.find_clusters(frame_with_cluster())
    hits = cf.steal_hits()
    values = hits.values
    records = hits.hits
    del hits
    gc.collect()
    _ = np.ones(10000)  # reuse freed memory, if any
    assert values.sum() == pytest.approx(260.0)
    assert records[0]["size"] == 9


def test_steal_hits_empties_the_finder():
    cf = VarClusterFinder((8, 8), 10.0)
    cf.find_clusters(frame_with_cluster())
    assert cf.total_clusters() == 1
    assert len(cf.steal_hits()) == 1
    assert cf.total_clusters() == 0
    empty = cf.steal_hits()
    assert len(empty) == 0
    assert empty.rows.shape == (0,)


def test_hits_accumulate_across_frames():
    cf = VarClusterFinder((8, 8), 10.0)
    cf.find_clusters(frame_with_cluster())
    cf.find_clusters(frame_with_cluster())
    hits = cf.steal_hits()
    assert len(hits) == 2
    assert hits.hits["offset"].tolist() == [0, 9]
    assert hits.values.shape == (18,)


def test_input_is_not_modified_by_default():
    img = frame_with_cluster()
    original = img.copy()
    cf = VarClusterFinder((8, 8), 10.0)
    cf.find_clusters(img)
    assert np.array_equal(img, original)


def test_erase_clusters_zeroes_cluster_and_ring():
    img = frame_with_cluster() + 1.0
    cf = VarClusterFinder((8, 8), 10.0)
    assert cf.erase_clusters is False
    cf.erase_clusters = True
    assert cf.erase_clusters is True
    cf.find_clusters(img)
    expected = np.ones((8, 8))
    expected[1:6, 2:7] = 0.0
    assert np.array_equal(img, expected)


def test_labeled_image():
    img = frame_with_cluster()
    img[7, 0] = 50.0
    cf = VarClusterFinder((8, 8), 10.0)
    cf.find_clusters(img)
    labels = cf.labeled
    assert labels.dtype == np.int32
    assert labels.shape == (8, 8)
    assert labels[3, 4] == 1
    assert labels[2, 3] == 1
    assert labels[7, 0] == 2
    assert labels[0, 0] == 0


def test_noise_map_from_float32_and_non_contiguous_arrays():
    # Regression test: the finder used to keep a view into the temporary
    # array created by the conversion.
    img = frame_with_cluster()
    img[4, 5] = 3.0  # neighbour between 2 and 5 sigma
    cf = VarClusterFinder((8, 8), 1000.0)
    noise = np.ones((8, 16), dtype=np.float32)[:, ::2]
    assert not noise.flags.c_contiguous
    cf.noise_map = noise
    del noise
    gc.collect()
    _ = np.ones((8, 8), dtype=np.float64) * 1e6  # reuse freed memory, if any

    cf.neighbour_factor = 2
    cf.find_clusters(img)
    hits = cf.steal_hits()
    assert len(hits) == 1
    assert hits.hits[0]["size"] == 9


def test_noise_map_property_round_trip():
    img = frame_with_cluster()
    cf = VarClusterFinder((8, 8), 1000.0)
    assert cf.noise_map is None
    cf.noise_map = np.ones((8, 8), dtype=np.float32)  # cast and copied
    assert cf.noise_map.dtype == np.float64
    assert np.array_equal(cf.noise_map, np.ones((8, 8)))
    cf.find_clusters(img)
    assert len(cf.steal_hits()) == 1
    cf.noise_map = None
    assert cf.noise_map is None
    cf.find_clusters(img)  # fixed threshold 1000: nothing
    assert len(cf.steal_hits()) == 0
    with pytest.raises(ValueError):
        cf.noise_map = [1.0, 2.0]  # not a 2D array


def test_seed_threshold_factor():
    img = np.zeros((8, 8))
    img[1, 1] = 4.0
    cf = VarClusterFinder((8, 8), 1000.0)
    cf.noise_map = np.ones((8, 8))
    cf.find_clusters(img)
    assert len(cf.steal_hits()) == 0
    cf.seed_factor = 3.5
    cf.find_clusters(img)
    assert len(cf.steal_hits()) == 1


def test_shape_mismatch_raises():
    cf = VarClusterFinder((8, 8), 10.0)
    with pytest.raises(RuntimeError):
        cf.find_clusters(np.zeros((8, 9)))
    with pytest.raises(RuntimeError):
        cf.noise_map = np.ones((9, 8))


def test_only_float64_frames_are_accepted():
    cf = VarClusterFinder((8, 8), 10.0)
    with pytest.raises(TypeError):
        cf.find_clusters(frame_with_cluster().astype(np.float32))
    with pytest.raises(ValueError):
        cf.find_clusters(np.zeros((8, 16))[:, ::2])


def test_large_blob_stores_every_pixel():
    img = np.full((512, 1024), 100.0)
    cf = VarClusterFinder((512, 1024), 10.0)
    cf.find_clusters(img)
    hits = cf.steal_hits()
    assert len(hits) == 1
    assert hits.hits[0]["size"] == 512 * 1024
    assert hits.values.shape == (512 * 1024,)
    assert hits.values.sum() == pytest.approx(100.0 * 512 * 1024)


def test_mask_excludes_pixels_from_seeding_and_growing():
    img = frame_with_cluster()
    img[7, 0] = 50.0
    mask = np.zeros((8, 8), dtype=bool)
    mask[3, 4] = True
    mask[7, 0] = True
    cf = VarClusterFinder((8, 8), 10.0)
    cf.mask = mask
    cf.find_clusters(img)
    hits = cf.steal_hits()
    assert len(hits) == 1
    assert hits.hits[0]["size"] == 8
    assert hits.hits[0]["energy"] == pytest.approx(160.0)
    labels = cf.labeled
    assert labels[3, 4] == -1
    assert labels[7, 0] == -1
    assert labels[2, 3] == 1
    cf.mask = None
    cf.find_clusters(img)
    assert len(cf.steal_hits()) == 2


def test_mask_property_round_trip_and_checks():
    cf = VarClusterFinder((8, 8), 10.0)
    assert cf.mask is None
    mask = np.zeros((8, 8), dtype=np.uint8)
    mask[1, 2] = 1
    cf.mask = mask  # integer arrays are cast to bool
    assert cf.mask.dtype == bool
    assert np.array_equal(cf.mask, mask.astype(bool))
    cf.mask = None
    assert cf.mask is None
    with pytest.raises(RuntimeError):
        cf.mask = np.zeros((8, 9), dtype=bool)
    with pytest.raises(ValueError):
        cf.mask = [True, False]  # not a 2D array


def test_clusters_touching_the_mask_can_be_found_afterwards():
    # The recipe from the docs: dilate the mask by one pixel, treat the
    # image border as masked and reduce per cluster.
    img = frame_with_cluster(row=3, col=4)
    img[6, 6] = 40.0  # single pixel away from the mask and the border
    mask = np.zeros((8, 8), dtype=bool)
    mask[3, 6] = True  # next to the cluster's right column
    cf = VarClusterFinder((8, 8), 10.0)
    cf.mask = mask
    cf.find_clusters(img)
    hits = cf.steal_hits()

    pad = np.pad(mask, 1, constant_values=True)
    near = np.zeros_like(mask)
    for dr in (0, 1, 2):
        for dc in (0, 1, 2):
            near |= pad[dr : dr + mask.shape[0], dc : dc + mask.shape[1]]
    touches = np.logical_or.reduceat(near[hits.rows, hits.cols], hits.hits["offset"])
    assert touches.tolist() == [True, False]
    assert hits.hits[~touches]["energy"].tolist() == [40.0]


def test_fixed_thresholds_for_seeds_and_neighbours():
    img = np.zeros((8, 8))
    img[1:6, 1:6] = 8.0
    img[2:5, 2:5] = 20.0
    img[3, 3] = 100.0
    cf = VarClusterFinder((8, 8), 50.0)
    assert cf.threshold == 50.0
    assert cf.neighbour_threshold == 50.0
    assert (cf.seed_factor, cf.neighbour_factor) == (5.0, 5.0)

    def size_of_first_hit():
        cf.find_clusters(img)
        hits = cf.steal_hits()
        assert len(hits) == 1
        return hits.hits[0]["size"]

    assert size_of_first_hit() == 1
    cf.neighbour_threshold = 10.0
    assert cf.neighbour_threshold == 10.0
    assert size_of_first_hit() == 9
    cf.neighbour_threshold = 5.0
    assert size_of_first_hit() == 25
    cf.neighbour_threshold = None
    assert cf.neighbour_threshold == 50.0
    assert size_of_first_hit() == 1
    cf.threshold = 15.0
    assert cf.neighbour_threshold == 15.0
    assert size_of_first_hit() == 9
    with pytest.raises(TypeError):
        cf.neighbour_threshold = "low"


def test_connectivity_property():
    img = np.zeros((8, 8))
    img[2, 2] = 50.0
    img[3, 3] = 40.0  # diagonal neighbour only
    cf = VarClusterFinder((8, 8), 10.0)
    assert cf.connectivity == 8
    cf.find_clusters(img)
    assert len(cf.steal_hits()) == 1
    cf.connectivity = 4
    assert cf.connectivity == 4
    cf.find_clusters(img)
    assert len(cf.steal_hits()) == 2
    with pytest.raises(RuntimeError):
        cf.connectivity = 6
    assert cf.connectivity == 4
