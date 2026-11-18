# SPDX-License-Identifier: MPL-2.0

import numpy as np
import pytest
from test_ClusterFinder import N_SAMPLES, SHAPE, _test_frames

from aare import (
    ClusterCollector,
    ClusterFile,
    ClusterFileSink,
    ClusterFinder,
    ClusterFinderMT,
    FastPedestal,
    read_state_header,
)

N_THREADS = 3


def _finder_mt(dtype=np.int32, n_sigma=4.1, n_threads=N_THREADS):
    return ClusterFinderMT(
        SHAPE,
        n_sigma=n_sigma,
        dtype=dtype,
        n_threads=n_threads,
        min_pedestal_samples=N_SAMPLES,
    )


def _process(finder, frames, first=0):
    """The first N_SAMPLES frames make the pedestal, the others are data."""
    for i, frame in enumerate(frames, start=first):
        if i < N_SAMPLES:
            finder.push_pedestal_frame(frame)
        else:
            finder.find_clusters(frame, i)


def _stop(finder, dtype=np.int32):
    """Stop the finder and return the number of clusters it found."""
    finder.stop()
    collector = ClusterCollector(finder, dtype=dtype)
    collector.stop()
    return sum(clusters.size for clusters in collector.steal_clusters())


def _dark_pedestal(seed, n_samples=8):
    pedestal = FastPedestal(*SHAPE, n_samples=n_samples)
    rng = np.random.default_rng(seed)
    for _ in range(n_samples):
        pedestal.add_init_frame(rng.integers(900, 910, size=SHAPE).astype(np.uint16))
    return pedestal


def _same_pedestal(a, b):
    return np.array_equal(a.mean(), b.mean()) and np.array_equal(a.std(), b.std())


@pytest.mark.parametrize("dtype", [np.int32, np.float64])
def test_cluster_finder_mt_continues_from_state_file(tmp_path, dtype):
    frames = _test_frames(41)
    uninterrupted = _finder_mt(dtype)
    _process(uninterrupted, frames)
    n_expected = _stop(uninterrupted, dtype)
    assert n_expected > 0

    original = _finder_mt(dtype)
    _process(original, frames[:21])
    n_found = _stop(original, dtype)
    assert original.save_state(tmp_path / "finder.aare") is None

    # The pedestals of the threads have drifted apart
    assert not _same_pedestal(original.get_pedestal(0), original.get_pedestal(1))

    restored = ClusterFinderMT.load_state(tmp_path / "finder.aare")
    assert type(restored) is type(original)
    assert restored.n_threads == N_THREADS
    _process(restored, frames[21:], first=21)
    n_found += _stop(restored, dtype)

    assert n_found == n_expected
    for thread_index in range(N_THREADS):
        np.testing.assert_array_equal(
            restored.pedestal(thread_index), uninterrupted.pedestal(thread_index)
        )
        np.testing.assert_array_equal(
            restored.noise(thread_index), uninterrupted.noise(thread_index)
        )


def test_cluster_finder_mt_load_state_takes_another_dtype(tmp_path):
    original = _finder_mt(np.int32)
    _process(original, _test_frames(12))
    _stop(original)
    original.save_state(tmp_path / "finder.aare")

    restored = ClusterFinderMT.load_state(tmp_path / "finder.aare", dtype=np.float64)
    restored.stop()
    assert type(restored) is type(_stopped(_finder_mt(np.float64)))
    np.testing.assert_array_equal(restored.pedestal(2), original.pedestal(2))

    with pytest.raises(ValueError, match="Unsupported"):
        ClusterFinderMT.load_state(tmp_path / "finder.aare", dtype=np.uint8)


def _stopped(finder):
    finder.stop()
    return finder


def test_cluster_finder_mt_state_header(tmp_path):
    finder = _finder_mt(np.float32, n_threads=2)
    _process(finder, _test_frames(8))
    _stop(finder, np.float32)
    finder.save_state(tmp_path / "finder.aare")

    header = read_state_header(tmp_path / "finder.aare")
    assert header["type"] == "ClusterFinderMT"
    state = header["state"]
    assert state["version"] == 1
    # Three data frames went to threads 0, 1 and 0
    assert state["next_thread"] == 1
    assert len(state["finders"]) == 2
    for finder_state in state["finders"]:
        assert finder_state["cluster_dtype"] == "<f4"
        assert finder_state["n_sigma"] == 4.1
        assert finder_state["pedestal"]["sum"]["shape"] == list(SHAPE)


def test_cluster_finder_mt_state_needs_a_stopped_finder(tmp_path):
    source = _finder_mt()
    _process(source, _test_frames(12))
    _stop(source)
    source.save_state(tmp_path / "finder.aare")
    pedestal = _dark_pedestal(seed=3)

    finder = _finder_mt()
    for call in (
        lambda: finder.save_state(tmp_path / "other.aare"),
        lambda: finder.load_state(tmp_path / "finder.aare"),
        lambda: finder.set_pedestal(pedestal),
        lambda: finder.get_pedestal(),
    ):
        with pytest.raises(RuntimeError, match="still running"):
            call()

    finder.stop()
    finder.stop()  # stopping a stopped finder does nothing
    assert finder.load_state(tmp_path / "finder.aare") is None
    for thread_index in range(N_THREADS):
        np.testing.assert_array_equal(
            finder.pedestal(thread_index), source.pedestal(thread_index)
        )

    # It finds clusters again once it is started
    finder.start()
    _process(finder, _test_frames(20)[12:], first=12)
    assert _stop(finder) > 0


def test_cluster_finder_mt_load_state_rejects_another_number_of_threads(tmp_path):
    source = _finder_mt(n_threads=2)
    _process(source, _test_frames(12))
    _stop(source)
    source.save_state(tmp_path / "two_threads.aare")

    finder = _stopped(_finder_mt(n_threads=3))
    with pytest.raises(RuntimeError, match="is for 2 threads, this finder has 3"):
        finder.load_state(tmp_path / "two_threads.aare")
    assert not finder.pedestal_ready()


def test_cluster_finder_mt_loads_the_state_file_of_a_cluster_finder(tmp_path):
    single = ClusterFinder(SHAPE, n_sigma=6.5, min_pedestal_samples=N_SAMPLES)
    for frame in _test_frames(N_SAMPLES, seed=4):
        single.push_pedestal_frame(frame)
    single.save_state(tmp_path / "single.aare")

    finder = _finder_mt()
    _process(finder, _test_frames(12))
    _stop(finder)
    before = [finder.pedestal(i).copy() for i in range(N_THREADS)]

    # To one thread
    finder.load_state(tmp_path / "single.aare", thread_index=1)
    np.testing.assert_array_equal(finder.pedestal(0), before[0])
    np.testing.assert_array_equal(finder.pedestal(1), single.pedestal)
    np.testing.assert_array_equal(finder.pedestal(2), before[2])

    # To all threads
    finder.load_state(tmp_path / "single.aare")
    for thread_index in range(N_THREADS):
        np.testing.assert_array_equal(finder.pedestal(thread_index), single.pedestal)

    with pytest.raises(RuntimeError, match="out of range"):
        finder.load_state(tmp_path / "single.aare", thread_index=N_THREADS)

    # The file of a ClusterFinderMT holds every thread
    finder.save_state(tmp_path / "finder.aare")
    with pytest.raises(RuntimeError, match="thread_index can only be given"):
        finder.load_state(tmp_path / "finder.aare", thread_index=0)


def test_cluster_finder_mt_sets_and_returns_pedestals(tmp_path):
    pedestal = _dark_pedestal(seed=5)
    other = _dark_pedestal(seed=6, n_samples=6)

    finder = _stopped(_finder_mt())
    assert finder.set_pedestal(pedestal) is None
    assert finder.pedestal_ready()
    for thread_index in range(N_THREADS):
        assert _same_pedestal(finder.get_pedestal(thread_index), pedestal)
        # The threshold follows from the noise and the nSigma of the finder
        np.testing.assert_array_equal(finder.noise(thread_index), pedestal.std())

    finder.set_pedestal(other, thread_index=2)
    assert _same_pedestal(finder.get_pedestal(), pedestal)
    assert _same_pedestal(finder.get_pedestal(1), pedestal)
    assert _same_pedestal(finder.get_pedestal(2), other)
    assert finder.get_pedestal(2).n_samples == 6

    # Through the state file of a pedestal
    finder.save_pedestal(tmp_path / "thread2.aare", thread_index=2)
    assert read_state_header(tmp_path / "thread2.aare")["type"] == "FastPedestal"
    assert _same_pedestal(FastPedestal.load_state(tmp_path / "thread2.aare"), other)

    finder.load_pedestal(tmp_path / "thread2.aare", thread_index=0)
    assert _same_pedestal(finder.get_pedestal(0), other)
    assert _same_pedestal(finder.get_pedestal(1), pedestal)
    finder.load_pedestal(tmp_path / "thread2.aare")
    for thread_index in range(N_THREADS):
        assert _same_pedestal(finder.get_pedestal(thread_index), other)

    with pytest.raises(RuntimeError, match="out of range"):
        finder.set_pedestal(pedestal, thread_index=N_THREADS)
    with pytest.raises(RuntimeError, match="out of range"):
        finder.get_pedestal(N_THREADS)

    # It finds clusters with the pedestals it was given
    finder.start()
    frame = np.full(SHAPE, 905, dtype=np.uint16)
    frame[4, 5] += 300
    for i in range(N_THREADS):
        finder.find_clusters(frame, i)
    assert _stop(finder) >= N_THREADS


def test_cluster_finder_returns_and_saves_its_pedestal(tmp_path):
    finder = ClusterFinder(SHAPE, n_sigma=4.1, min_pedestal_samples=N_SAMPLES)
    for frame in _test_frames(N_SAMPLES):
        finder.push_pedestal_frame(frame)

    pedestal = finder.get_pedestal()
    assert type(pedestal) is type(FastPedestal(*SHAPE))
    np.testing.assert_array_equal(pedestal.mean(), finder.pedestal)
    np.testing.assert_array_equal(pedestal.std(), finder.noise)

    # A copy: the finder goes on without it
    finder.find_clusters(_test_frames(N_SAMPLES + 1)[-1])
    assert not np.array_equal(pedestal.mean(), finder.pedestal)

    finder.save_pedestal(tmp_path / "pedestal.aare")
    other = ClusterFinder(SHAPE, n_sigma=4.1, min_pedestal_samples=N_SAMPLES)
    other.load_pedestal(tmp_path / "pedestal.aare")
    np.testing.assert_array_equal(other.pedestal, finder.pedestal)


def test_clusters_can_be_stolen_from_a_running_collector():
    finder = _finder_mt()
    collector = ClusterCollector(finder)
    frames = _test_frames(N_SAMPLES + 20)
    _process(finder, frames[:N_SAMPLES])

    # After sync() the collector has one ClusterVector for each data frame
    _process(finder, frames[N_SAMPLES : N_SAMPLES + 12], first=N_SAMPLES)
    finder.sync()
    first = collector.steal_clusters()
    assert sorted(c.frame_number for c in first) == list(
        range(N_SAMPLES, N_SAMPLES + 12)
    )

    # Stolen clusters are gone, later ones are still collected
    _process(finder, frames[N_SAMPLES + 12 :], first=N_SAMPLES + 12)
    finder.sync()
    second = collector.steal_clusters()
    assert sorted(c.frame_number for c in second) == list(
        range(N_SAMPLES + 12, N_SAMPLES + 20)
    )

    finder.stop()
    collector.stop()
    assert len(collector.steal_clusters()) == 0


N_PEDESTAL_FRAMES = 8


def _finder_with_pedestal():
    finder = ClusterFinderMT(
        SHAPE, n_sigma=5, n_threads=N_THREADS, min_pedestal_samples=N_PEDESTAL_FRAMES
    )
    for i in range(N_PEDESTAL_FRAMES):
        value = 903 if i % 2 else 907
        finder.push_pedestal_frame(np.full(SHAPE, value, dtype=np.uint16))
    return finder


def _find(finder, frame_numbers):
    frame = np.full(SHAPE, 905, dtype=np.uint16)
    frame[4, 5] += 300
    for frame_number in frame_numbers:
        finder.find_clusters(frame, frame_number)


def _frame_numbers(cluster_vectors):
    return sorted(clusters.frame_number for clusters in cluster_vectors)


def test_collector_can_be_started_again():
    finder = _finder_with_pedestal()
    collector = ClusterCollector(finder)

    _find(finder, range(4))
    finder.sync()
    collector.stop()
    collector.stop()

    collector.start()
    collector.start()
    _find(finder, range(4, 8))
    finder.sync()
    assert _frame_numbers(collector.steal_clusters()) == list(range(8))

    finder.stop()
    collector.stop()


def test_collector_can_be_dropped_without_stop():
    finder = _finder_with_pedestal()
    collector = ClusterCollector(finder)
    _find(finder, range(4))
    finder.sync()
    # The collector keeps the finder alive and stops its own thread
    del finder
    del collector


def _frame_numbers_in(path):
    with ClusterFile(path) as file:
        frame_numbers = []
        while (clusters := file.read_frame()) is not None:
            frame_numbers.append(clusters.frame_number)
    return sorted(frame_numbers)


def test_file_sink_writes_the_clusters_of_all_frames(tmp_path):
    path = tmp_path / "clusters.clust"
    finder = _finder_with_pedestal()
    sink = ClusterFileSink(finder, path)
    _find(finder, range(20))
    finder.stop()
    sink.stop()
    sink.stop()
    assert _frame_numbers_in(path) == list(range(20))


def test_file_sink_writes_clusters_found_before_it_was_created(tmp_path):
    path = tmp_path / "clusters.clust"
    finder = _finder_with_pedestal()
    _find(finder, range(20))
    finder.stop()
    sink = ClusterFileSink(finder, path)
    sink.stop()
    assert _frame_numbers_in(path) == list(range(20))


def test_file_sink_can_be_dropped_without_stop(tmp_path):
    path = tmp_path / "clusters.clust"
    finder = _finder_with_pedestal()
    sink = ClusterFileSink(finder, path)
    _find(finder, range(20))
    finder.sync()
    # The sink keeps the finder alive, stops its own thread and closes the
    # file
    del finder
    del sink
    assert _frame_numbers_in(path) == list(range(20))


def test_file_sink_raises_if_the_file_cannot_be_opened(tmp_path):
    finder = _finder_with_pedestal()
    with pytest.raises(RuntimeError, match="Could not open"):
        ClusterFileSink(finder, tmp_path / "missing" / "clusters.clust")
    finder.stop()


def test_running_finder_can_be_dropped():
    finder = _finder_with_pedestal()
    _find(finder, range(20))
    del finder


def test_finder_start_and_stop_can_be_repeated():
    finder = _finder_with_pedestal()
    collector = ClusterCollector(finder)
    finder.start()
    _find(finder, range(4))
    finder.stop()
    finder.stop()
    finder.start()
    _find(finder, range(4, 8))
    finder.stop()
    collector.stop()
    assert _frame_numbers(collector.steal_clusters()) == list(range(8))
