Interpolation
==============

Interpolation class for :math:`\eta` Interpolation. 

:math:`\eta`-Functions: 
---------------------------

.. doxygenstruct:: aare::Eta2
    :members: 
    :undoc-members: 
    :private-members: 

.. doxygenfunction:: aare::calculate_eta2(const ClusterVector<ClusterType>&)

.. doxygenfunction:: aare::calculate_eta2(const Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>&)

.. doxygenfunction:: aare::calculate_eta3(const ClusterVector<Cluster<T, 3,3, CoordType>>&)

.. doxygenfunction:: aare::calculate_eta3(const Cluster<T, 3, 3, CoordType>&)

.. doxygenfunction:: aare::calculate_cross_eta3(const ClusterVector<Cluster<T, 3,3, CoordType>>&)

.. doxygenfunction:: aare::calculate_cross_eta3(const Cluster<T, 3, 3, CoordType>&)

Interpolation class: 
---------------------

.. doxygenclass:: aare::Interpolator
   :members:
   :undoc-members:
   :private-members:


