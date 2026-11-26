# SPDX-License-Identifier: MPL-2.0
import numpy as np
import pytest

from aare import ClusterConverter, _aare, calculate_eta2


def make_clusters(values, frame_number=0):
    """3x3 int32 clusters, one per value, with every pixel set to that value."""
    cv = _aare.ClusterVector_Cluster3x3i()
    cv.frame_number = frame_number
    for i, value in enumerate(values):
        cv.push_back(_aare.Cluster3x3i(i, 2 * i, np.full(9, value, dtype=np.int32)))
    return cv


def test_returns_float32_clusters_and_keeps_the_input():
    cv = make_clusters([5, -3, 0], frame_number=42)
    result = ClusterConverter(seed=1)(cv)

    assert isinstance(result, _aare.ClusterVector_Cluster3x3f)
    assert result.frame_number == 42
    arr = np.array(result)
    assert arr["data"].dtype == np.float32
    np.testing.assert_array_equal(arr["x"], [0, 1, 2])
    np.testing.assert_array_equal(arr["y"], [0, 2, 4])
    np.testing.assert_array_equal(np.array(cv)["data"][:, 0, 0], [5, -3, 0])


def test_rounded_values_stay_within_half_a_step():
    values = np.array([-40, -1, 0, 1, 40] * 400)
    data = np.array(ClusterConverter(truncated=False, seed=2)(make_clusters(values)))["data"]
    offset = data - values[:, None, None]

    assert offset.min() >= -0.5
    assert offset.max() <= 0.5
    assert offset.max() - offset.min() > 0.99
    assert abs(offset.mean()) < 0.01


def test_truncated_values_move_away_from_zero():
    values = np.array([-40, 0, 40] * 700)
    data = np.array(ClusterConverter(truncated=True, seed=3)(make_clusters(values)))["data"]
    negative, zero, positive = data[values == -40], data[values == 0], data[values == 40]

    assert -41 < negative.min() and negative.max() <= -40
    assert 40 <= positive.min() and positive.max() <= 41
    assert -1 < zero.min() < -0.99 and 0.99 < zero.max() < 1
    assert abs(negative.mean() + 40.5) < 0.01
    assert abs(positive.mean() - 40.5) < 0.01
    assert abs(zero.mean()) < 0.02


def test_seed_makes_the_result_reproducible():
    cv = make_clusters([3] * 20)
    converter = ClusterConverter(seed=4)
    first = np.array(converter(cv))["data"]
    second = np.array(converter(cv))["data"]
    same = np.array(ClusterConverter(seed=4)(cv))["data"]
    other = np.array(ClusterConverter(seed=5)(cv))["data"]
    unseeded = np.array(ClusterConverter()(cv))["data"]

    np.testing.assert_array_equal(first, same)
    assert not np.array_equal(first, second)  # the generator moves on
    assert not np.array_equal(first, other)
    assert not np.array_equal(first, unseeded)


def test_result_does_not_depend_on_the_chunk_size():
    values = np.arange(-50, 50)
    whole = np.array(ClusterConverter(seed=6)(make_clusters(values)))["data"]

    converter = ClusterConverter(seed=6)
    chunks = [np.array(converter(make_clusters(values[i:i + 7])))["data"]
              for i in range(0, values.size, 7)]
    np.testing.assert_array_equal(np.concatenate(chunks), whole)


def test_other_integer_cluster_types():
    cv = _aare.ClusterVector_Cluster2x2i()
    cv.push_back(_aare.Cluster2x2i(1, 2, np.array([1, 2, 3, 4], dtype=np.int32)))
    assert isinstance(ClusterConverter()(cv), _aare.ClusterVector_Cluster2x2f)

    cv16 = _aare.ClusterVector_Cluster3x3i16()
    cv16.push_back(_aare.Cluster3x3i16(1, 2, np.arange(9, dtype=np.int16)))
    result = ClusterConverter(truncated=True, seed=6)(cv16)
    assert isinstance(result, _aare.ClusterVector_Cluster3x3f)
    np.testing.assert_array_equal(np.floor(np.array(result)["data"][0].ravel())[1:], np.arange(1, 9))


def test_invalid_arguments():
    with pytest.raises(TypeError):
        ClusterConverter(2.5)  # keyword only
    with pytest.raises(TypeError):
        ClusterConverter(gain="two")
    with pytest.raises(ValueError):
        ClusterConverter(gain=np.ones(4))
    with pytest.raises(ValueError):
        ClusterConverter(gain=np.ones((2, 2, 2)))
    with pytest.raises(TypeError):
        ClusterConverter()(_aare.ClusterVector_Cluster3x3f())
    with pytest.raises(TypeError):
        ClusterConverter()(np.zeros(3))


def test_eta_of_truncated_data_has_no_pile_up():
    # Truncated integers put eta at exactly 0 or 1 whenever a neighbour is 0.
    rng = np.random.default_rng(7)
    cv = _aare.ClusterVector_Cluster3x3i()
    for _ in range(3000):
        data = np.trunc(rng.normal(0, 3, 9)).astype(np.int32)
        data[4] = 1000
        cv.push_back(_aare.Cluster3x3i(5, 5, data))

    eta_int = calculate_eta2(cv)["x"]
    eta_float = calculate_eta2(ClusterConverter(truncated=True, seed=8)(cv))["x"]

    assert np.mean((eta_int == 0) | (eta_int == 1)) > 0.1
    assert np.mean((eta_float == 0) | (eta_float == 1)) == 0


def test_rounds_by_default():
    data = np.array(ClusterConverter(seed=9)(make_clusters([40] * 500)))["data"]
    assert data.min() >= 39.5 and data.max() <= 40.5
    assert data.min() < 39.6


@pytest.mark.parametrize("gain", [2.5, np.float32(2.5), 4])
def test_uniform_gain_in_the_same_pass(gain):
    cv = make_clusters([40] * 50, frame_number=5)
    plain = np.array(ClusterConverter(truncated=True, seed=10)(cv))["data"]
    scaled = ClusterConverter(gain=gain, truncated=True, seed=10)(cv)

    assert scaled.frame_number == 5
    np.testing.assert_allclose(np.array(scaled)["data"], plain / float(gain), rtol=1e-6)


def test_gain_map_in_the_same_pass():
    gain = 1.0 + np.arange(6)[:, None] + 10 * np.arange(8)[None, :]  # [y, x]
    gain.setflags(write=False)
    cv = _aare.ClusterVector_Cluster3x3i()
    centers = [(3, 2), (1, 1), (6, 4), (0, 2), (7, 2), (3, 0), (3, 5)]  # (x, y)
    for x, y in centers:
        cv.push_back(_aare.Cluster3x3i(x, y, np.full(9, 1000, dtype=np.int32)))

    arr = np.array(ClusterConverter(gain=gain, seed=11)(cv))
    for cluster, (x, y) in zip(arr[:3], centers[:3]):
        expected = gain[y - 1:y + 2, x - 1:x + 2]
        assert np.abs(cluster["data"] * expected - 1000).max() <= 0.51
    # Clusters reaching beyond the map are kept and cleared
    assert arr.size == 7
    assert np.all(arr["data"][3:] == 0)
    np.testing.assert_array_equal(arr["x"], [x for x, _ in centers])


def test_gain_map_is_copied():
    gain = np.full((4, 4), 2.0)
    converter = ClusterConverter(gain=gain, seed=13)
    gain[:] = 1000.0
    cv = _aare.ClusterVector_Cluster3x3i()
    cv.push_back(_aare.Cluster3x3i(1, 1, np.full(9, 100, dtype=np.int32)))
    assert np.abs(np.array(converter(cv))["data"] - 50).max() <= 0.25


def test_gain_map_matches_the_cluster_file_convention(tmp_path):
    # Same pixels and same gain as the deprecated ClusterFile.set_gain_map,
    # without the cast back to integers.
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
        with pytest.warns(DeprecationWarning, match="ClusterConverter"):
            f.set_gain_map(gain)
        from_file = np.array(f.read_frame())["data"]
    with ClusterFile(fname) as f:
        converted = np.array(ClusterConverter(gain=gain, seed=12)(f.read_frame()))["data"]

    assert np.abs(converted - from_file).max() <= 1.0
