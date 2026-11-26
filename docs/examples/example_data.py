# SPDX-License-Identifier: MPL-2.0
"""Generate the input files used by the examples in this directory.

The examples call these functions in blocks that sphinx-gallery hides from
the rendered page, so that the page shows only the code a user would write
for their own data. The file name does not start with ``plot_``, so it is
not run as an example itself.
"""

import json
import math
from pathlib import Path

import numpy as np

from aare import ClusterFileSink, ClusterFinderMT

_SQRT2 = math.sqrt(2.0)

# Header written by the slsReceiver in front of the pixel data of every frame
# of a raw file, 112 bytes. Matches aare::DetectorHeader.
_DETECTOR_HEADER = np.dtype(
    [
        ("frameNumber", np.uint64),
        ("expLength", np.uint32),
        ("packetNumber", np.uint32),
        ("bunchId", np.uint64),
        ("timestamp", np.uint64),
        ("modId", np.uint16),
        ("row", np.uint16),
        ("column", np.uint16),
        ("reserved", np.uint16),
        ("debug", np.uint32),
        ("roundRNumber", np.uint16),
        ("detType", np.uint8),
        ("version", np.uint8),
        ("packetMask", np.uint8, 64),
    ]
)


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


def write_jungfrau_raw_file(base_name, n_frames=20, photon_adu=500.0, seed=2):
    """Simulate a measurement with a Jungfrau 500k and write it as a raw file.

    The module has 512 x 1024 pixels and is read out over two UDP interfaces,
    each sending one half of the rows. The receiver then writes one data file
    per interface next to the master file, so this creates:

    - ``<base_name>_master_0.json`` with the metadata, in the layout of
      version 8.1 of the master file,
    - ``<base_name>_d0_f0_0.raw`` with rows 0 to 255,
    - ``<base_name>_d1_f0_0.raw`` with rows 256 to 511.

    Each data file holds, per frame, a 112 byte header followed by
    256 x 1024 ``uint16`` pixels.

    The frames show powder diffraction rings on a weak background. A pixel
    holds a 14 bit ADC value in bits 0 to 13 and its gain stage in bits 14 and
    15. In the highest gain, G0, the ADC value is the pedestal plus
    ``photon_adu`` per photon. A pixel that collects more charge than G0 can
    hold switches to G1, where the ADC value falls with the signal, which
    happens in the brightest ring. The pixels at the borders between the
    2 x 4 chips are twice as large and collect twice the photons.

    Returns the path of the master file. Frame numbers start at 1.
    """
    rng = np.random.default_rng(seed)
    rows, cols = 512, 1024
    port_rows = rows // 2
    period_ns = 2_000_000

    # Mean number of photons per pixel and frame: rings around a beam centre
    # below the module, on a background that falls off with the radius
    row, col = np.mgrid[:rows, :cols]
    radius = np.hypot(row - 620.0, col - 400.0)
    photons = 0.3 * np.exp(-radius / 250.0)
    for ring_radius, width, peak in [
        (180.0, 3.0, 4.0),
        (330.0, 3.5, 60.0),
        (450.0, 4.0, 1.5),
        (560.0, 4.0, 8.0),
        (700.0, 5.0, 0.8),
    ]:
        photons += peak * np.exp(-0.5 * ((radius - ring_radius) / width) ** 2)

    # Double size pixels at the chip borders
    photons[[255, 256], :] *= 2.0
    photons[:, [255, 256, 511, 512, 767, 768]] *= 2.0

    pedestal_g0 = rng.normal(2500.0, 60.0, (rows, cols))
    pedestal_g1 = rng.normal(14500.0, 40.0, (rows, cols))
    g0_to_g1 = -30.0  # ratio of the gains, G1 has the opposite sign

    header = np.zeros(1, dtype=_DETECTOR_HEADER)
    header["packetNumber"] = 64  # 8192 bytes of pixel data per packet
    header["detType"] = 3  # Jungfrau
    header["version"] = 1
    header["packetMask"][0, :8] = 0xFF  # all 64 packets received

    base_name = Path(base_name)
    data_files = [
        open(f"{base_name}_d{port}_f0_0.raw", "wb") for port in range(2)
    ]
    for i in range(n_frames):
        signal = photon_adu * rng.poisson(photons)
        g0 = pedestal_g0 + signal + rng.normal(0.0, 12.0, (rows, cols))
        g1 = pedestal_g1 + signal / g0_to_g1 + rng.normal(0.0, 3.0, (rows, cols))

        switched = g0 > 15000.0
        adc = np.clip(np.rint(np.where(switched, g1, g0)), 0, 0x3FFF)
        frame = adc.astype(np.uint16) | (switched.astype(np.uint16) << 14)

        header["frameNumber"] = i + 1
        header["timestamp"] = i * period_ns // 100  # 10 MHz clock
        for port, f in enumerate(data_files):
            header["row"] = port
            f.write(header.tobytes())
            f.write(frame[port * port_rows : (port + 1) * port_rows].tobytes())

    for f in data_files:
        f.close()

    master = {
        "Version": 8.1,
        "Timestamp": "Thu Nov 26 14:02:47 2026",
        "Detector Type": "Jungfrau",
        "Timing Mode": "auto",
        "Geometry": {"x": 1, "y": 2},
        "Image Size": port_rows * cols * 2,
        "Pixels": {"x": cols, "y": port_rows},
        "Max Frames Per File": 10000,
        "Frame Discard Policy": "nodiscard",
        "Frame Padding": 1,
        "Scan Parameters": {
            "enable": 0,
            "dacInd": 0,
            "start offset": 0,
            "stop offset": 0,
            "step size": 0,
            "dac settle time ns": 0,
        },
        "Total Frames": n_frames,
        "Receiver Rois": [
            {"xmin": 0, "xmax": cols - 1, "ymin": 0, "ymax": rows - 1}
        ],
        "Exposure Time": "10us",
        "Acquisition Period": "2ms",
        "Number of UDP Interfaces": 2,
        "UDP Ports Type": ["bottom", "top"],
        "UDP Ports Disabled": [],
        "Number of Rows": rows,
        "Readout Speed": "half_speed",
        "Frames in File": n_frames,
        "Additional JSON Header": {},
    }
    master_fname = Path(f"{base_name}_master_0.json")
    master_fname.write_text(json.dumps(master, indent=4))
    return master_fname
