# SPDX-License-Identifier: MPL-2.0

import numpy as np

from aare import ClusterCollector, ClusterFinderMT

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
    collector = ClusterCollector(finder)

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

