# This file is used to get the version of the package from the VERSION file
from importlib.metadata import PackageNotFoundError, version
from pathlib import Path

try:
    __version__ = version('aare')
except PackageNotFoundError:
    __version__ = Path(__file__).parents[2].joinpath('VERSION').read_text().strip()