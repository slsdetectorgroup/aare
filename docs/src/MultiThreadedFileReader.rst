MultiThreadedFileReader
=======================

``MultiThreadedFileReader`` reads one chunk per worker on each call. Each
worker owns an independent :cpp:class:`aare::File`, while all workers write
into non-overlapping regions of the destination buffer. The output is ordered
by frame index and successive calls advance through the file.

.. code-block:: cpp

   #include "aare/MultiThreadedFileReader.hpp"

   // Four workers, chunks of 128 frames, and at most 10,000 frames.
   aare::MultiThreadedFileReader reader(path, 4, 128, 10'000);
   while (reader.remaining_frames() != 0) {
       // Contains at most 4 * 128 frames.
       auto batch = reader.read();
       process(batch);
   }

Omit the final argument to read every frame in the source. An explicit value
of zero requests an empty result. The low-level ``read_into`` overload avoids
an allocation when the caller already owns a buffer of at least
``reader.next_read_bytes()`` bytes. Use ``read_all()`` to read every frame
remaining from the current position, and ``seek()`` to reposition the reader.
Call ``close()`` to release all worker file handles early.

.. note::

   Multiple workers do not guarantee faster reads. Performance depends on the
   storage device and file format, so the thread count and chunk size should be
   benchmark-driven.

.. doxygenclass:: aare::MultiThreadedFileReader
   :members:
   :undoc-members:
   :private-members:
