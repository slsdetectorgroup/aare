PixelHistogram
==============

``PixelHistogram`` accumulates one histogram per detector pixel from 2D
``float64`` images. The public ``PixelHistogram`` name is an alias for
``PixelHistogram_d``, which stores bin counts as ``float64``.

.. note::

    ``PixelHistogram`` is designed for fast filling from 2D images, utilizing multiple threads, 
    and contiguous storage of a specific type. No over/underflow bins are provided. For generic use cases consider
    boost-histogram. (https://boost-histogram.readthedocs.io/en/latest/)


Storage-specific variants are also available when a smaller or integer count
type is preferred:

* ``PixelHistogram_d``: ``float64`` storage
* ``PixelHistogram_f``: ``float32`` storage
* ``PixelHistogram_u64``: ``uint64`` storage
* ``PixelHistogram_u32``: ``uint32`` storage
* ``PixelHistogram_u16``: ``uint16`` storage
* ``PixelHistogram_u8``: ``uint8`` storage

**Saving and restoring the counts**

``save_state()`` writes the range and the counts to a state file, including
the images that were still waiting to be filled.
``from_state_file(filename, n_threads=1, max_pending=16)`` of a variant
returns a new histogram with the image shape, the number of bins and the
range stored in the file, and ``hist.load_state(filename)`` replaces the
counts of an existing histogram with the same shape, bins and range. The
counts are not converted: a file loads only into the variant with the
storage dtype that wrote it, which
``aare.read_state_header(filename)["state"]["values"]["dtype"]`` tells. The
file format is described in :doc:`../../StateFile`.

.. code:: python

    hist.save_state("hist.aare")

    hist = PixelHistogram_u16.from_state_file("hist.aare", n_threads=8)
    other.load_state("hist.aare")

.. py:currentmodule:: aare

.. autoclass:: PixelHistogram
    :members:
    :undoc-members:
    :show-inheritance:
    :inherited-members:

Showing API for PixelHistogram_d, all variant share the same API.

.. autoclass:: aare._aare.PixelHistogram_d
    :members:
    :undoc-members:
    :show-inheritance:
    :inherited-members:
    :noindex:

