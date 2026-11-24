ClusterFinder
=============

``ClusterFinder`` combines a :doc:`ClusterFinderMT` with a cluster sink. The
sink type is a template parameter and must derive from ``ClusterSink``:
``ClusterVectorSink`` keeps the clusters in memory, ``ClusterFileSink`` writes
them to a binary cluster file.

.. code-block:: cpp

    #include "aare/ClusterVectorSink.hpp"
    #include "aare/ClusterFileSink.hpp"
    #include "aare/ClusterFinder.hpp"

    using ClusterType = aare::Cluster<int32_t, 3, 3>;

    // in memory
    aare::ClusterFinder<aare::ClusterVectorSink<ClusterType>> cf({400, 400});
    // ... push_pedestal_frame / find_clusters
    cf.stop();
    auto clusters = cf.steal_clusters();

    // to file, the trailing arguments construct the sink
    aare::ClusterFinder<aare::ClusterFileSink<ClusterType>> writer(
        {400, 400}, 5.0, 2000, 3, 16, 1000, "out.clust");


.. doxygenclass:: aare::ClusterFinder
   :members:
   :undoc-members:

.. doxygenclass:: aare::ClusterSink
   :members:
   :undoc-members:
