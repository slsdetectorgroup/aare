# SPDX-License-Identifier: MPL-2.0
"""Generate the input files used by the examples in this directory.

The examples call these functions in blocks that sphinx-gallery hides from
the rendered page, so that the page shows only the code a user would write
for their own data. The file name does not start with ``plot_``, so it is
not run as an example itself.
"""

import math

import numpy as np

from aare import ClusterFileSink, ClusterFinderMT

_SQRT2 = math.sqrt(2.0)


def _pixel_fractions(position, first_edge, sigma):
    """Share of a Gaussian charge cloud collected by three adjacent pixels.

    The pixels start at ``first_edge``, ``first_edge + 1`` and
    ``first_edge + 2``.
    """
    cdf = [
        0.5 * (1.0 + math.erf((first_edge + i - position) / (sigma * _SQRT2)))
        for i in range(4)
    ]
    return np.diff(cdf)


def write_cluster_file(
    fname,
    image_size=(200, 200),
    n_frames=1000,
    photons_per_frame=40,
    photon_adu=600.0,
    noise_adu=12.0,
    charge_cloud_sigma=0.17,
    seed=1,
):
    """Simulate a measurement and write its clusters to ``fname``.

    Raw frames are simulated and passed through ``ClusterFinderMT``, which
    writes the 3x3 ``int32`` clusters it finds with a ``ClusterFileSink``.
    The file therefore holds what the cluster finder produces for a real
    measurement: frame numbers, noise in every pixel of a cluster, clusters
    lost at the threshold and clusters with two overlapping photons.

    Each frame holds, in ADU:

    - a pedestal that differs from pixel to pixel,
    - Gaussian electronic noise with a standard deviation of ``noise_adu``,
    - a Poisson distributed number of photons, ``photons_per_frame`` on
      average, that each deposit ``photon_adu``. Most photons come from a
      beam spot and the rest are spread evenly over the sensor. The charge of
      a photon is a Gaussian cloud with a standard deviation of
      ``charge_cloud_sigma`` pixels, so a photon that lands near a pixel
      border shares its charge with the neighbouring pixels.

    Frame numbers start at 1.
    """
    rng = np.random.default_rng(seed)
    rows, cols = image_size
    pedestal = rng.normal(3000.0, 30.0, image_size)

    def simulate_frame(n_photons):
        frame = pedestal + rng.normal(0.0, noise_adu, image_size)

        # Photon positions in pixel units: 80 % in the beam spot
        in_beam = rng.random(n_photons) < 0.8
        x = np.where(
            in_beam,
            rng.normal(0.55 * cols, 0.20 * cols, n_photons),
            rng.uniform(0, cols, n_photons),
        )
        y = np.where(
            in_beam,
            rng.normal(0.50 * rows, 0.12 * rows, n_photons),
            rng.uniform(0, rows, n_photons),
        )

        for px, py in zip(x, y):
            col, row = math.floor(px), math.floor(py)
            if not (1 <= col < cols - 1 and 1 <= row < rows - 1):
                continue
            share_x = _pixel_fractions(px, col - 1, charge_cloud_sigma)
            share_y = _pixel_fractions(py, row - 1, charge_cloud_sigma)
            frame[row - 1 : row + 2, col - 1 : col + 2] += photon_adu * np.outer(
                share_y, share_x
            )

        return np.clip(np.rint(frame), 0, 65535).astype(np.uint16)

    n_pedestal_frames = 200
    # A single thread keeps the frames in the file in the order they were taken
    finder = ClusterFinderMT(
        image_size,
        cluster_size=(3, 3),
        dtype=np.int32,
        n_sigma=5,
        n_threads=1,
        min_pedestal_samples=n_pedestal_frames,
    )
    sink = ClusterFileSink(finder, fname, dtype=np.int32)

    for _ in range(n_pedestal_frames):
        finder.push_pedestal_frame(simulate_frame(0))

    for frame_number in range(1, n_frames + 1):
        frame = simulate_frame(rng.poisson(photons_per_frame))
        finder.find_clusters(frame, frame_number)

    # The file is complete once both have stopped
    finder.stop()
    sink.stop()
