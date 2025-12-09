Interpolation 
==============

Interpolation class for :math:`\eta` Interpolation. 

The Interpolator class provides methods to interpolate the positions of photons based on their :math:`\eta` values. 

.. warning:: 
   The interpolation might lead to erroneous photon positions for clusters at the boarders of a frame. Make sure to filter out such cases. 

Below is an example of the Eta class of type ``double``. Supported are ``Etaf`` of type ``float`` and ``Etai`` of type ``int``. 

.. autoclass:: aare._aare.Etad
    :members:
    :private-members:

.. note::
    The corner value ``c`` is only relevant when one uses ``calculate_eta_2`` or ``calculate_full_eta2``. Otherwise its default value is ``cTopLeft``. 

Supported are the following :math:`\eta`-functions: 

.. py:currentmodule:: aare

.. image:: ../../../figures/Eta2x2.png
    :target: ../../../figures/Eta2x2.png
    :width: 650px
    :align: center
    :alt: Eta2x2

.. math:: 
    \begin{equation*}
   {\color{blue}{\eta_x}} = \frac{Q_{1,1}}{Q_{1,0} + Q_{1,1}} \quad \quad 
   {\color{green}{\eta_y}} = \frac{Q_{1,1}}{Q_{0,1} + Q_{1,1}}
   \end{equation*}    

.. autofunction:: calculate_eta2

.. image:: ../../../figures/Eta2x2Full.png
    :target: ../../../figures/Eta2x2Full.png
    :width: 650px
    :align: center
    :alt: Eta2x2 Full 

.. math:: 
    \begin{equation*}
        {\color{blue}{\eta_x}}  = \frac{Q_{0,1} + Q_{1,1}}{\sum_i^{2}\sum_j^{2}Q_{i,j}} \quad \quad
        {\textcolor{green}{\eta_y}} = \frac{Q_{1,0} + Q_{1,1}}{\sum_i^{2}\sum_j^{2}Q_{i,j}}
    \end{equation*} 

.. autofunction:: calculate_full_eta2

.. image:: ../../../figures/Eta3x3.png
    :target: ../../../figures/Eta3x3.png
    :width: 650px
    :align: center
    :alt: Eta3x3

.. math::
    \begin{equation*}
         {\color{blue}{\eta_x}} = \frac{\sum_{i}^{3} Q_{i,2} - \sum_{i}^{3} Q_{i,0}}{\sum_{i}^{3}\sum_{j}^{3} Q_{i,j}} \quad \quad
        {\color{green}{\eta_y}} = \frac{\sum_{j}^{3} Q_{2,j} - \sum_{j}^{3} Q_{0,j}}{\sum_{i}^{3}\sum_{j}^{3} Q_{i,j}}
    \end{equation*}

.. autofunction:: calculate_eta3

.. image:: ../../../figures/Eta3x3Cross.png
    :target: ../../../figures/Eta3x3Cross.png
    :width: 650px
    :align: center
    :alt: Cross Eta3x3

.. math:: 
    \begin{equation*}
       {\color{blue}{\eta_x}} = \frac{Q_{1,2} - Q_{1,0}}{Q_{1,0} +  Q_{1,1} + Q_{1,0}} \quad \quad
        {\color{green}{\eta_y}} = \frac{Q_{0,2} - Q_{0,1}}{Q_{0,1} +  Q_{1,1} + Q_{1,2}}
    \end{equation*}

.. autofunction:: calculate_cross_eta3


Interpolation class for :math:`\eta`-Interpolation 
----------------------------------------------------

.. Warning:: 
    Make sure to use the same :math:`\eta`-function during interpolation as given by the joint :math:`\eta`-distribution passed to the constructor. 

.. py:currentmodule:: aare

.. autoclass:: Interpolator
    :special-members: __init__
    :members:
    :undoc-members:
    :inherited-members:

