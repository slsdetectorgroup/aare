# SPDX-License-Identifier: MPL-2.0
import numpy as np
import pytest

from aare import ClusterFinderCUDA, _cuda_available, find_cluster_views_batched_iter

pytestmark = pytest.mark.skipif(
    not _cuda_available(), reason="aare built without AARE_CUDA")

SHAPE = (32, 32)
BATCH = 4
# Pixel that gets a photon in every data frame, away from the border.
HIT = (16, 16)


@pytest.fixture
def cf():
    cf = ClusterFinderCUDA(SHAPE, n_streams=2, min_pedestal_samples=10)
    rng = np.random.default_rng(42)
    for _ in range(20):
        cf.push_pedestal_frame(
            rng.normal(1000, 5, SHAPE).astype(np.uint16))
    return cf


@pytest.fixture
def frames():
    rng = np.random.default_rng(7)
    f = rng.normal(1000, 5, (BATCH, *SHAPE)).astype(np.uint16)
    f[:, HIT[0], HIT[1]] = 3000
    return f


def test_collect_view_exposes_batch(cf, frames):
    tok = cf.submit_batch(frames, first_frame=100)
    with cf.collect_view(tok) as v:
        assert v.valid
        assert v.n_frames == BATCH
        assert v.first_frame == 100
        assert all(v.count(i) >= 1 for i in range(BATCH))
    assert not v.valid


def test_released_view_frees_its_slot(cf, frames):
    # Cycles both slots twice. pybind11 move-constructs every returned
    # BatchView, so a move that drops the slot index leaves the slot held
    # forever and the third submit_batch() throws.
    for i in range(2 * 2):
        tok = cf.submit_batch(frames, first_frame=i * BATCH)
        v = cf.collect_view(tok)
        v.release()
        assert not v.valid


def test_dropped_view_frees_its_slot(cf, frames):
    # The destructor releases, so a view that is never bound is harmless.
    for i in range(2 * 2):
        tok = cf.submit_batch(frames, first_frame=i * BATCH)
        cf.collect_view(tok)


def test_held_view_blocks_its_slot(cf, frames):
    held = cf.collect_view(cf.submit_batch(frames))
    # The other slot is still free.
    with cf.collect_view(cf.submit_batch(frames)):
        pass
    with pytest.raises(RuntimeError, match="held by a BatchView"):
        cf.submit_batch(frames)
    held.release()
    with cf.collect_view(cf.submit_batch(frames)):
        pass


def test_views_batched_iter_pipelines_all_chunks(cf, frames):
    data = np.concatenate([frames] * 3)
    seen = []
    for v in find_cluster_views_batched_iter(cf, data, first_frame=10,
                                             chunk=BATCH):
        seen.append((v.first_frame, v.n_frames))
    assert seen == [(10, BATCH), (10 + BATCH, BATCH), (10 + 2 * BATCH, BATCH)]
