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

