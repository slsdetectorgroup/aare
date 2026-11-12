ClusterFinderCUDA
=================

.. py:currentmodule:: aare

Requires a build configured with ``-DAARE_CUDA=ON``. On a CPU-only build the
factories raise ``RuntimeError``.

.. autofunction:: ClusterFinderCUDA

.. autofunction:: ClusterFinderCUDAGraph

.. autofunction:: find_cluster_views_batched_iter

Returned classes
----------------

The factories return one class per cluster size and value type, named
``ClusterFinderCUDA_Cluster<N>x<N><t>`` and
``ClusterFinderCUDAGraph_Cluster<N>x<N><t>`` (``t``: ``i`` int32, ``f``
float32, ``d`` float64). All sizes and types share the API below, shown for
3x3 int32. Frames are ``(nrows, ncols)`` or ``(n_frames, nrows, ncols)``
arrays, converted to C-contiguous ``uint16`` if needed.

.. py:class:: ClusterFinderCUDA_Cluster3x3i

   Returned by :py:func:`ClusterFinderCUDA`.

   .. rubric:: Pedestal

   .. py:method:: push_pedestal_frame(frame)

      Add a frame to the host pedestal.

   .. py:method:: clear_pedestal()

      Reset the host pedestal.

   .. py:property:: pedestal

      Host pedestal mean, ``(nrows, ncols)`` float64.

   .. py:property:: noise

      Host pedestal RMS, ``(nrows, ncols)`` float64.

   .. py:method:: device_pedestal(stream=0)

      Pedestal mean on the GPU for one stream: the one the kernel decides with
      and updates each frame, unlike ``pedestal``.

   .. py:method:: device_noise(stream=0)

      Pedestal RMS on the GPU for one stream, computed as the kernel does:
      ``sqrt(max(sum2/n - mean^2, 0))``.

   .. py:property:: nSigma

      Threshold in units of pedestal RMS. Read and write.

   .. rubric:: One frame at a time

   .. py:method:: find_clusters(frame, frame_number=0)

      Find the clusters of one frame and add them to an internal
      ``ClusterVector``. Take them with ``steal_clusters()``.

   .. py:method:: steal_clusters(realloc_same_capacity=True)

      Return the clusters found since the last call as a ``ClusterVector``
      and start a new, empty one (with the same capacity if
      ``realloc_same_capacity``).

   .. rubric:: Batches

   .. py:method:: find_clusters_batched(frames, first_frame=0)

      Process ``frames`` over ``n_streams`` CUDA streams, overlapping copies
      and kernels. Returns a list with one ``ClusterVector`` per frame.

   .. py:method:: submit_batch(frames, first_frame=0)

      Queue a batch and return at once with a
      :py:class:`ClusterFinderCUDA_Cluster3x3i_BatchToken`. At most two
      batches can be in flight; give each its own input array, so filling the
      next one does not overwrite a copy still running.

   .. py:method:: collect(token)

      Wait for a submitted batch and return one ``ClusterVector`` per frame.
      Frees the batch slot.

   .. py:method:: collect_view(token)

      Like ``collect()``, but copies nothing: returns a
      :py:class:`ClusterFinderCUDA_Cluster3x3i_BatchView` that reads the
      pinned result buffer. The slot stays taken until the view is released.

   .. py:property:: batch_chunk

      Frames per chunk inside ``find_clusters_batched()``. 0 (default): about
      8 chunks per batch, rounded up to a multiple of ``n_streams`` so each
      frame meets the same stream (and device pedestal) as without chunking.
      Set it to the batch size to turn chunking off. Read and write.

   .. py:method:: chunk_size_for(n_frames)

      The chunk size ``find_clusters_batched()`` would use for ``n_frames``.
      Use it to pipeline ``submit_batch()``/``collect_view()`` the same way.

   .. py:method:: reserve_output_slots(n_frames)

      Allocate both pinned result buffers for batches of ``n_frames`` now,
      instead of in the first ``submit_batch()``. Processes nothing. Call it
      before starting a timer: pinning takes about 1 µs per 4 kB page.

   .. rubric:: Pinned input

   .. py:method:: register_input_buffer(frames)

      Pin ``frames`` so copies from it (and from its slices) run at full
      bandwidth. Replaces the buffer registered before. An array can be
      registered by only one finder at a time.

   .. py:method:: unregister_input_buffer()

      Unpin the registered buffer.

   .. py:method:: pin_buffer(array)

      Pin another array, keeping any already pinned (e.g. the two alternating
      input arrays of a ``submit_batch()`` loop).

   .. py:method:: unpin_buffer(array)

      Unpin an array pinned with ``pin_buffer()``.

   .. rubric:: Timing

   .. py:method:: avg_kernel_time_ms()

      Average kernel time per frame in ms; NaN unless built with
      ``time_kernels=True``. Only meaningful with ``n_streams=1``: with more
      streams it includes time queued behind other streams' kernels.

   .. py:method:: kernel_timing_enabled()

      Whether the finder was built with ``time_kernels=True``.

   .. py:method:: reset_timers()

      Reset the kernel timing counters.

.. py:class:: ClusterFinderCUDA_Cluster3x3i_BatchToken

   Handle returned by ``submit_batch()``. Pass it once to ``collect()`` or
   ``collect_view()``.

.. py:class:: ClusterFinderCUDA_Cluster3x3i_BatchView

   Results of one batch, read in place from the finder's pinned buffer.
   Returned by ``collect_view()``. Use it as a context manager or call
   ``release()``; anything kept after that must be copied out.

   .. py:property:: n_frames

      Frames in the batch.

   .. py:property:: first_frame

      Frame number of the first frame.

   .. py:property:: valid

      False once released.

   .. py:property:: total_clusters

      Clusters in the whole batch.

   .. py:property:: counts

      Clusters per frame, uint32 array.

   .. py:method:: count(frame_index)

      Clusters in one frame.

   .. py:method:: sums()

      Sum of each cluster in the batch, computed in C++ from the pinned buffer
      without building the clusters. The fast path for spectra.

   .. py:method:: frame_data(frame_index)

      ``(n_clusters, N*N)`` view of one frame's cluster pixels. No copy; valid
      until the view is released.

   .. py:method:: frame_xy(frame_index)

      ``(n_clusters, 2)`` view of one frame's cluster centres as ``(x, y)``.
      No copy; valid until the view is released.

   .. py:method:: release()

      Give the slot back to the finder. Called on ``__exit__`` and on
      destruction.

.. py:class:: ClusterFinderCUDAGraph_Cluster3x3i

   Returned by :py:func:`ClusterFinderCUDAGraph`. A subset of
   :py:class:`ClusterFinderCUDA_Cluster3x3i`, with the same behaviour unless
   noted.

   .. py:method:: push_pedestal_frame(frame)
   .. py:method:: clear_pedestal()
   .. py:property:: pedestal
   .. py:property:: noise
   .. py:property:: nSigma
   .. py:method:: find_clusters(frame, frame_number=0)
   .. py:method:: steal_clusters(realloc_same_capacity=True)
   .. py:method:: find_clusters_batched(frames, first_frame=0)

      Runs one recorded CUDA graph per stream.

   .. py:method:: register_input_buffer(frames)
   .. py:method:: unregister_input_buffer()
   .. py:method:: avg_kernel_time_ms()

      Always 0.0: kernels are not timed. Time ``find_clusters_batched()``
      from the outside instead.

   .. py:method:: reset_timers()
