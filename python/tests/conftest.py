import os
from pathlib import Path
import pytest



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


