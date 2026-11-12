# SPDX-License-Identifier: MPL-2.0
# The docs are built without CUDA, so autodoc can't import the CUDA classes and
# their pages list the API by hand. Check that each page names every public
# member of its bound class, and nothing else.
import re
from pathlib import Path

import pytest

from aare import _cuda_available

pytestmark = pytest.mark.skipif(
    not _cuda_available(), reason="aare built without AARE_CUDA")

DOCS = Path(__file__).resolve().parents[2] / "docs" / "src"

# (page, class documented on it); one instance stands for all its variants.
PAGES = [
    ("python/cluster/pyClusterFinderCUDA.rst", name)
    for name in ("ClusterFinderCUDA_Cluster3x3i",
                 "ClusterFinderCUDA_Cluster3x3i_BatchToken",
                 "ClusterFinderCUDA_Cluster3x3i_BatchView",
                 "ClusterFinderCUDAGraph_Cluster3x3i")
]


def documented_members(page, cls):
    text = (DOCS / page).read_text()
    # The class block runs to the next directive at column 0.
    block = re.search(rf"^\.\. py:class:: {cls}$.*?(?=^\.\. |\Z)", text,
                      re.M | re.S)
    assert block, f"{cls} is not documented in {page}"
    return set(re.findall(r"^ +\.\. py:(?:method|property|attribute):: (\w+)",
                          block.group(0), re.M))


@pytest.mark.parametrize("page, cls", PAGES)
def test_page_lists_the_public_api(page, cls):
    if not (DOCS / page).exists():
        pytest.skip("docs sources not available")
    from aare import _aare_cuda
    bound = getattr(_aare_cuda, cls)
    public = {n for n in dir(bound) if not n.startswith("_")}
    assert documented_members(page, cls) == public
