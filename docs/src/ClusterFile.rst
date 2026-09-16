ClusterFile
=============

Use ``frames()`` to iterate over complete frames, including empty frames and
frames whose clusters are all rejected by ROI or noise filtering. Each result
keeps its stored frame number; missing frame numbers are not synthesized.

.. code-block:: cpp

    using ClusterType = aare::Cluster<int32_t, 3, 3>;
    aare::ClusterFile<ClusterType> file("clusters.clust");
    for (auto &frame : file.frames()) {
        process_frame(frame.frame_number(), frame);
    }

Use ``chunks()`` for the constructor's chunk size, or ``chunks(10000)`` to
request a different size for that traversal. The size must be positive and
counts selected clusters after filtering. Chunks may split or combine frames,
so their frame numbers are not reliable per-cluster metadata. Empty chunks are
not yielded; only the final chunk can contain fewer clusters than requested.
Both ranges apply the configured gain map and report incomplete files as
errors, just like the explicit read methods.

Ranges borrow the file and consume its current position without rewinding.
Constructing a range does not read; ``begin()`` reads the first result and
increment reads the next. Breaking a loop does not read ahead. Use one
traversal at a time and separate files for independent cursors. Switching to
frame reads after a chunk stops partway through a frame raises an error until
the remaining clusters in that frame have been read.

The file must outlive its ranges and iterators and must not be moved while
they are in use. Iterators are move only and support C++17 range-based loops,
not algorithms requiring copyable STL input iterators. References to a result
last until advancement or iterator destruction. Move a result out with
``auto retained = std::move(frame)`` to retain it. Frame iteration reuses the
current vector's storage when it has not been moved out.

.. doxygenclass:: aare::ClusterFile
   :members:
   :undoc-members:
