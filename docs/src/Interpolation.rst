Interpolation
==============

Interpolation class for :math:`\eta` Interpolation. 

:math:`\eta`-Functions: 
---------------------------

.. doxygenstruct:: aare::Eta2
    :members: 
    :undoc-members: 
    :private-members: 

.. note::
    The corner value ``c`` is only relevant when one uses ``calculate_eta_2`` or ``calculate_full_eta2``. Otherwise its default value is ``cTopLeft``. 

.. doxygenfunction:: aare::calculate_eta2(const ClusterVector<ClusterType>&)

.. doxygenfunction:: aare::calculate_eta2(const Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>&)

.. doxygenfunction:: aare::calculate_full_eta2(const ClusterVector<ClusterType>&)

.. doxygenfunction:: aare::calculate_full_eta2(const Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>&)

.. doxygenfunction:: aare::calculate_eta3(const ClusterVector<Cluster<T, 3,3, CoordType>>&)

.. doxygenfunction:: aare::calculate_eta3(const Cluster<T, 3, 3, CoordType>&)

.. doxygenfunction:: aare::calculate_cross_eta3(const ClusterVector<Cluster<T, 3,3, CoordType>>&)

.. doxygenfunction:: aare::calculate_cross_eta3(const Cluster<T, 3, 3, CoordType>&)

Interpolation class: 
---------------------

.. Warning:: 
    Make sure to use the same :math:`\eta`-function during interpolation as given by the joint :math:`\eta`-distribution passed to the constructor. 

.. doxygenclass:: aare::Interpolator
   :members:
   :undoc-members:
   :private-members:


