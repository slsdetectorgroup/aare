Interpolation
==============

Interpolation class for :math:`\eta` Interpolation. 

The Interpolator class provides methods to interpolate the positions of photons based on their :math:`\eta` values. 

:math:`\eta`-Functions: 
---------------------------

.. doxygenstruct:: aare::Eta2
    :members: 
    :undoc-members: 
    :private-members: 

.. note::
    The corner value ``c`` is only relevant when one uses ``calculate_eta_2`` or ``calculate_full_eta2``. Otherwise its default value is ``cTopLeft``. 

Supported are the following :math:`\eta`-functions: 


.. image:: ../figures/Eta2x2.png
    :target: ../figures/Eta2x2.png
    :width: 650px
    :align: center
    :alt: Eta2x2

.. math:: 
    \begin{equation*}
   {\color{blue}{\eta_x}} = \frac{Q_{1,1}}{Q_{1,0} + Q_{1,1}} \quad \quad 
   {\color{green}{\eta_y}} = \frac{Q_{1,1}}{Q_{0,1} + Q_{1,1}}
   \end{equation*}


.. doxygenfunction:: aare::calculate_eta2(const ClusterVector<ClusterType>&)

.. doxygenfunction:: aare::calculate_eta2(const Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>&) 

.. image:: ../figures/Eta2x2Full.png
    :target: ../figures/Eta2x2Full.png
    :width: 650px
    :align: center
    :alt: Eta2x2 Full 

.. math:: 
    \begin{equation*}
        {\color{blue}{\eta_x}}  = \frac{Q_{0,1} + Q_{1,1}}{\sum_i^{2}\sum_j^{2}Q_{i,j}} \quad \quad
        {\textcolor{green}{\eta_y}} = \frac{Q_{1,0} + Q_{1,1}}{\sum_i^{2}\sum_j^{2}Q_{i,j}}
    \end{equation*} 


.. doxygenfunction:: aare::calculate_full_eta2(const ClusterVector<ClusterType>&)

.. doxygenfunction:: aare::calculate_full_eta2(const Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>&)

.. image:: ../figures/Eta3x3.png
    :target: ../figures/Eta3x3.png
    :width: 650px
    :align: center
    :alt: Eta3x3

.. math::
    \begin{equation*}
         {\color{blue}{\eta_x}} = \frac{\sum_{i}^{3} Q_{i,2} - \sum_{i}^{3} Q_{i,0}}{\sum_{i}^{3}\sum_{j}^{3} Q_{i,j}} \quad \quad
        {\color{green}{\eta_y}} = \frac{\sum_{j}^{3} Q_{2,j} - \sum_{j}^{3} Q_{0,j}}{\sum_{i}^{3}\sum_{j}^{3} Q_{i,j}}
    \end{equation*}

.. doxygenfunction:: aare::calculate_eta3(const ClusterVector<Cluster<T, 3,3, CoordType>>&)

.. doxygenfunction:: aare::calculate_eta3(const Cluster<T, 3, 3, CoordType>&)

.. image:: ../figures/Eta3x3Cross.png
    :target: ../figures/Eta3x3Cross.png
    :width: 650px
    :align: center
    :alt: Cross Eta3x3

.. math:: 
    \begin{equation*}
       {\color{blue}{\eta_x}} = \frac{Q_{1,2} - Q_{1,0}}{Q_{1,0} +  Q_{1,1} + Q_{1,0}} \quad \quad
        {\color{green}{\eta_y}} = \frac{Q_{0,2} - Q_{0,1}}{Q_{0,1} +  Q_{1,1} + Q_{1,2}}
    \end{equation*}

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


