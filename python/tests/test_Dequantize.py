# SPDX-License-Identifier: MPL-2.0
import numpy as np
import pytest

from aare import _aare, calculate_eta2, dequantize


def make_clusters(values, frame_number=0):
    """3x3 int32 clusters, one per value, with every pixel set to that value."""
    cv = _aare.ClusterVector_Cluster3x3i()
    cv.frame_number = frame_number
    for i, value in enumerate(values):
        cv.push_back(_aare.Cluster3x3i(i, 2 * i, np.full(9, value, dtype=np.int32)))
    return cv


def test_returns_float32_clusters_and_keeps_the_input():
    cv = make_clusters([5, -3, 0], frame_number=42)
    result = dequantize(cv, truncated=False, seed=1)

    assert isinstance(result, _aare.ClusterVector_Cluster3x3f)
    assert result.frame_number == 42
    arr = np.array(result)
    assert arr["data"].dtype == np.float32
    np.testing.assert_array_equal(arr["x"], [0, 1, 2])
    np.testing.assert_array_equal(arr["y"], [0, 2, 4])
    np.testing.assert_array_equal(np.array(cv)["data"][:, 0, 0], [5, -3, 0])


def test_rounded_values_stay_within_half_a_step():
    values = np.array([-40, -1, 0, 1, 40] * 400)
    data = np.array(dequantize(make_clusters(values), truncated=False, seed=2))["data"]
    offset = data - values[:, None, None]

    assert offset.min() >= -0.5
    assert offset.max() <= 0.5
    assert offset.max() - offset.min() > 0.99
    assert abs(offset.mean()) < 0.01


def test_truncated_values_move_away_from_zero():
    values = np.array([-40, 0, 40] * 700)
    data = np.array(dequantize(make_clusters(values), truncated=True, seed=3))["data"]
    negative, zero, positive = data[values == -40], data[values == 0], data[values == 40]

    assert -41 < negative.min() and negative.max() <= -40
    assert 40 <= positive.min() and positive.max() <= 41
    assert -1 < zero.min() < -0.99 and 0.99 < zero.max() < 1
    assert abs(negative.mean() + 40.5) < 0.01
    assert abs(positive.mean() - 40.5) < 0.01
    assert abs(zero.mean()) < 0.02


def test_seed_makes_the_result_reproducible():
    cv = make_clusters([3] * 20)
    first = np.array(dequantize(cv, truncated=False, seed=4))["data"]
    same = np.array(dequantize(cv, truncated=False, seed=4))["data"]
    other = np.array(dequantize(cv, truncated=False, seed=5))["data"]
    unseeded = np.array(dequantize(cv, truncated=False))["data"]

    np.testing.assert_array_equal(first, same)
    assert not np.array_equal(first, other)
    assert not np.array_equal(first, unseeded)


def test_out_is_filled_and_returned():
    out = _aare.ClusterVector_Cluster3x3f()
    result = dequantize(make_clusters([7] * 10, frame_number=3), truncated=False, out=out)

    assert result is out
    assert out.size == 10
    assert out.frame_number == 3
    assert abs(np.array(out)["data"] - 7).max() <= 0.5

    dequantize(make_clusters([9] * 4), truncated=False, out=out)
    assert out.size == 4


def test_other_integer_cluster_types():
    cv = _aare.ClusterVector_Cluster2x2i()
    cv.push_back(_aare.Cluster2x2i(1, 2, np.array([1, 2, 3, 4], dtype=np.int32)))
    assert isinstance(dequantize(cv, truncated=False), _aare.ClusterVector_Cluster2x2f)

    cv16 = _aare.ClusterVector_Cluster3x3i16()
    cv16.push_back(_aare.Cluster3x3i16(1, 2, np.arange(9, dtype=np.int16)))
    result = dequantize(cv16, truncated=True, seed=6)
    assert isinstance(result, _aare.ClusterVector_Cluster3x3f)
    np.testing.assert_array_equal(np.floor(np.array(result)["data"][0].ravel())[1:], np.arange(1, 9))


def test_invalid_arguments():
    cv = make_clusters([1])
    with pytest.raises(TypeError):
        dequantize(cv, True)  # keyword only
    with pytest.raises(TypeError):
        dequantize(cv, gain="two")
    with pytest.raises(ValueError):
        dequantize(cv, gain=np.ones(4))
    with pytest.raises(TypeError):
        dequantize(_aare.ClusterVector_Cluster3x3f(), truncated=False)
    with pytest.raises(TypeError):
        dequantize(cv, truncated=False, out=_aare.ClusterVector_Cluster3x3d())
    with pytest.raises(TypeError):
        dequantize(cv, truncated=False, out=_aare.ClusterVector_Cluster2x2f())


def test_eta_of_truncated_data_has_no_pile_up():
    # Truncated integers put eta at exactly 0 or 1 whenever a neighbour is 0.
    rng = np.random.default_rng(7)
    cv = _aare.ClusterVector_Cluster3x3i()
    for _ in range(3000):
        data = np.trunc(rng.normal(0, 3, 9)).astype(np.int32)
        data[4] = 1000
        cv.push_back(_aare.Cluster3x3i(5, 5, data))

    eta_int = calculate_eta2(cv)["x"]
    eta_float = calculate_eta2(dequantize(cv, truncated=True, seed=8))["x"]

    assert np.mean((eta_int == 0) | (eta_int == 1)) > 0.1
    assert np.mean((eta_float == 0) | (eta_float == 1)) == 0


def test_rounds_by_default():
    data = np.array(dequantize(make_clusters([40] * 500), seed=9))["data"]
    assert data.min() >= 39.5 and data.max() <= 40.5
    assert data.min() < 39.6


@pytest.mark.parametrize("gain", [2.5, np.float32(2.5), 4])
def test_uniform_gain_in_the_same_pass(gain):
    cv = make_clusters([40] * 50, frame_number=5)
    plain = np.array(dequantize(cv, truncated=True, seed=10))["data"]
    scaled = dequantize(cv, gain=gain, truncated=True, seed=10)

    assert scaled.frame_number == 5
    np.testing.assert_allclose(np.array(scaled)["data"], plain / float(gain), rtol=1e-6)


def test_gain_map_in_the_same_pass():
    gain = 1.0 + np.arange(6)[:, None] + 10 * np.arange(8)[None, :]  # [y, x]
    gain.setflags(write=False)
    cv = _aare.ClusterVector_Cluster3x3i()
    centers = [(3, 2), (1, 1), (6, 4), (0, 2), (7, 2), (3, 0), (3, 5)]  # (x, y)
    for x, y in centers:
        cv.push_back(_aare.Cluster3x3i(x, y, np.full(9, 1000, dtype=np.int32)))

    arr = np.array(dequantize(cv, gain=gain, seed=11))
    for cluster, (x, y) in zip(arr[:3], centers[:3]):
        expected = gain[y - 1:y + 2, x - 1:x + 2]
        assert np.abs(cluster["data"] * expected - 1000).max() <= 0.51
    # Clusters reaching beyond the map are kept and cleared
    assert arr.size == 7
    assert np.all(arr["data"][3:] == 0)
    np.testing.assert_array_equal(arr["x"], [x for x, _ in centers])


def test_gain_map_matches_the_cluster_file_convention(tmp_path):
    # Same pixels and same gain as ClusterFile.set_gain_map, without the
    # cast back to integers.
    from aare import ClusterFile

    gain = 1.0 + np.arange(12)[:, None] + 10 * np.arange(12)[None, :]
    cv = make_clusters([0] * 5)
    arr = np.array(cv, copy=False)
    arr["x"], arr["y"] = [3, 5, 7, 2, 9], [4, 6, 2, 8, 3]
    arr["data"] = 10000
    fname = tmp_path / "gain.clust"
    with ClusterFile(fname, mode="w") as f:
        f.write_frame(cv)

    with ClusterFile(fname) as f:
        f.set_gain_map(gain)
        from_file = np.array(f.read_frame())["data"]
    with ClusterFile(fname) as f:
        converted = np.array(dequantize(f.read_frame(), gain=gain, seed=12))["data"]

    assert np.abs(converted - from_file).max() <= 1.0
