MultiThreadedFileReader
=======================

.. py:currentmodule:: aare.experimental

The reader returns a NumPy array with shape ``(frames, rows, cols)`` and
preserves the source pixel dtype. Each iteration reads at most
``n_threads * chunk_size`` frames—one chunk per worker. File I/O runs with the
Python GIL released.

.. code-block:: python

    from aare.experimental import MultiThreadedFileReader

    with MultiThreadedFileReader(
        "frames.npy", n_threads=4, chunk_size=128, total_frames=10_000
    ) as reader:
        for frames in reader:
            process(frames)

Call ``read()`` directly for the next batch, or ``read_all()`` for all frames
remaining from the current position. ``tell()`` and ``seek()`` expose the
iteration position. The context manager closes all worker files on exit.
``close()`` is also available for explicit cleanup and may be called
repeatedly.

.. autoclass:: MultiThreadedFileReader
    :members:
    :undoc-members:
    :show-inheritance:
