ClusterVector
=============

.. doxygenclass:: aare::ClusterVector
   :members:
   :undoc-members:
   :private-members:



.. doxygenclass:: aare::ClusterVector< Cluster< T, ClusterSizeX, ClusterSizeY, CoordType > > 
   :members:
   :undoc-members:
   :private-members:
   

**Free Functions:** 

.. doxygenfunction:: aare::reduce_to_3x3(const ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>>&)
   
.. doxygenfunction:: aare::reduce_to_2x2(const ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>>&)
   

**Gain correction:**

``ClusterFile::set_gain_map`` is deprecated. Use a
:ref:`Dequantizer <Dequantizer_C++API>` to apply a gain map to integer clusters.
