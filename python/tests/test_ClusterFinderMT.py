# SPDX-License-Identifier: MPL-2.0

import numpy as np
import pytest

from aare import ClusterVectorSink, ClusterFile, ClusterFileSink, ClusterFinderMT

SHAPE = (10, 12)
N_SAMPLES = 8
N_THREADS = 3


def _finder_with_pedestal():
    finder = ClusterFinderMT(
        SHAPE, n_sigma=5, n_threads=N_THREADS, min_pedestal_samples=N_SAMPLES
    )
    for i in range(N_SAMPLES):
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


def test_clusters_can_be_stolen_from_a_running_collector():
    finder = _finder_with_pedestal()
    collector = ClusterVectorSink(finder)

    # After sync() the collector has one ClusterVector for each data frame
    _find(finder, range(12))
    finder.sync()
    assert _frame_numbers(collector.steal_clusters()) == list(range(12))

    # Stolen clusters are gone, later ones are still collected
    _find(finder, range(12, 20))
    finder.sync()
    assert _frame_numbers(collector.steal_clusters()) == list(range(12, 20))

    finder.stop()
    collector.stop()
    assert len(collector.steal_clusters()) == 0


def test_collector_can_be_started_again():
    finder = _finder_with_pedestal()
    collector = ClusterVectorSink(finder)

    _find(finder, range(4))
    finder.sync()
    collector.stop()
    collector.stop()

    collector.collect(finder)
    with pytest.raises(RuntimeError, match="already collecting"):
        collector.collect(finder)
    _find(finder, range(4, 8))
    finder.sync()
    assert _frame_numbers(collector.steal_clusters()) == list(range(8))

    finder.stop()
    collector.stop()


def test_collector_can_be_dropped_without_stop():
    finder = _finder_with_pedestal()
    collector = ClusterVectorSink(finder)
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
    with pytest.raises(RuntimeError, match="could not open"):
        ClusterFileSink(finder, tmp_path / "missing" / "clusters.clust")
    finder.stop()


def test_running_finder_can_be_dropped():
    finder = _finder_with_pedestal()
    _find(finder, range(20))
    del finder


def test_finder_start_and_stop_can_be_repeated():
    finder = _finder_with_pedestal()
    collector = ClusterVectorSink(finder)
    finder.start()
    _find(finder, range(4))
    finder.stop()
    finder.stop()
    finder.start()
    _find(finder, range(4, 8))
    finder.stop()
    collector.stop()
    assert _frame_numbers(collector.steal_clusters()) == list(range(8))
