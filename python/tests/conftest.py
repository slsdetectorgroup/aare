# SPDX-License-Identifier: MPL-2.0
import os
from pathlib import Path
import pytest
import numpy as np 



def pytest_addoption(parser):
    parser.addoption(
        "--with-data", action="store_true", default=False, help="Run tests that require additional data"
    )


def pytest_configure(config):
    config.addinivalue_line("markers", "withdata: mark test as needing image files to run")


def pytest_collection_modifyitems(config, items):
    if config.getoption("--with-data"):
        return
    skip = pytest.mark.skip(reason="need --with-data option to run")
    for item in items:
        if "withdata" in item.keywords:
            item.add_marker(skip)


@pytest.fixture
def test_data_path():
    env_value = os.environ.get("AARE_TEST_DATA")
    if not env_value:
        raise RuntimeError("Environment variable AARE_TEST_DATA is not set or is empty")

    return Path(env_value)


@pytest.fixture
def create_temporary_jungfrau_files(tmp_path):
    """Factory fixture: writes a Jungfrau raw file set into tmp_path and returns the path of the first file."""

    JungfrauDataHeader = np.dtype([("framenum", np.uint64), ("bunchid", np.uint64)])

    def _create(num_frames, frame_generator, num_files=1):
        master_path = tmp_path / "aare-raw"
        master_path.mkdir(exist_ok=True)

        frames_per_file = num_frames // num_files

        for file_index in range(num_files):
            start_frame = file_index * frames_per_file
            end_frame = num_frames if file_index == num_files - 1 else start_frame + frames_per_file

            with open(master_path / f"JungfrauTest_{file_index:06}.dat", "wb") as f:
                for frame in range(start_frame, end_frame):
                    # header
                    f.write(np.array([(frame, 0)], dtype=JungfrauDataHeader).tobytes())
                    f.write(np.ascontiguousarray(frame_generator(frame), dtype=np.uint16).tobytes())

        return master_path / "JungfrauTest_000000.dat"

    return _create
