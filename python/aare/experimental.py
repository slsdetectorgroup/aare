# SPDX-License-Identifier: MPL-2.0
"""Experimental APIs that may change without notice."""

from ._aare.experimental import MultiThreadedFileReader

__all__ = ["MultiThreadedFileReader"]

try:
    from ._aare.experimental import fit_minuit2

    __all__.append("fit_minuit2")
except ImportError:  # built without AARE_MINUIT2
    pass
