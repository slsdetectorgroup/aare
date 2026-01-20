Interpolation 
==============

The Interpolation class implements the :math:`\eta`-interpolation method.
This interpolation technique is based on charge sharing: for detected photon hits (e.g. clusters), it refines the estimated photon hit using information from neighboring pixels.

See :ref:`Interpolation_C++API` for a more elaborate documentation and explanation of the method. 

:math:`\eta`-Functions: 
--------------------------

Below is an example of the Eta class of type ``double``. Supported are ``Etaf`` of type ``float`` and ``Etai`` of type ``int``. 

.. autoclass:: aare._aare.Etad
    :members:
    :private-members:

.. note::
    The corner value ``c`` is only relevant when one uses ``calculate_eta_2`` or ``calculate_full_eta2``. Otherwise its default value is ``cTopLeft``. 

Supported are the following :math:`\eta`-functions: 

:math:`\eta`-Function on 2x2 Clusters: 
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

The :math:`\eta` values can range between 0,1. Note they only range between 0,1 because the position of the center pixel (red) can change. 
If the center pixel is in the bottom left pixel :math:`\eta_x` will be close to zero. If the center pixel is in the bottom right pixel :math:`\eta_y` will be close to 1. 

.. autofunction:: calculate_eta2

Full :math:`\eta`-Function on 2x2 Clusters: 
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. image:: ../../../figures/Eta2x2Full.png
    :target: ../../../figures/Eta2x2Full.png
    :width: 650px
    :align: center
    :alt: Eta2x2 Full 

.. math:: 
    \begin{equation*}
        {\color{blue}{\eta_x}}  = \frac{Q_{0,1} + Q_{1,1}}{\sum_{i=0}^{1}\sum_{j=0}^{1}Q_{i,j}} \quad \quad
        {\textcolor{green}{\eta_y}} = \frac{Q_{1,0} + Q_{1,1}}{\sum_{i=0}^{1}\sum_{j=0}^{1}Q_{i,j}}
    \end{equation*} 

The :math:`\eta` values can range between 0,1. Note they only range between 0,1 because the position of the center pixel (red) can change. 
If the center pixel is in the bottom left pixel :math:`\eta_x` will be close to zero. If the center pixel is in the bottom right pixel :math:`\eta_y` will be close to 1. 

.. autofunction:: calculate_full_eta2

Full :math:`\eta`-Function on 3x3 Clusters: 
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. image:: ../../../figures/Eta3x3.png
    :target: ../../../figures/Eta3x3.png
    :width: 650px
    :align: center
    :alt: Eta3x3

.. math::
    \begin{equation*}
         {\color{blue}{\eta_x}} = \frac{\sum_{i=0}^{2} Q_{i,2} - \sum_{i=0}^{2} Q_{i,0}}{\sum_{i=0}^{2}\sum_{j}^{3} Q_{i,j}} \quad \quad
        {\color{green}{\eta_y}} = \frac{\sum_{j=0}^{2} Q_{2,j} - \sum_{j=0}^{2} Q_{0,j}}{\sum_{i=0}^{2}\sum_{j}^{3} Q_{i,j}}
    \end{equation*}

The :math:`\eta` values can range between -0.5,0.5. 

.. autofunction:: calculate_eta3

Cross :math:`\eta`-Function on 3x3 Clusters: 
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. image:: ../../../figures/Eta3x3Cross.png
    :target: ../../../figures/Eta3x3Cross.png
    :width: 650px
    :align: center
    :alt: Cross Eta3x3

.. math:: 
    \begin{equation*}
       {\color{blue}{\eta_x}} = \frac{Q_{1,2} - Q_{1,0}}{Q_{1,0} +  Q_{1,1} + Q_{1,2}} \quad \quad
        {\color{green}{\eta_y}} = \frac{Q_{0,2} - Q_{0,1}}{Q_{0,1} +  Q_{1,1} + Q_{2,1}}
    \end{equation*}

The :math:`\eta` values can range between -0.5,0.5. 

.. autofunction:: calculate_cross_eta3


Interpolation class for :math:`\eta`-Interpolation 
----------------------------------------------------

.. Warning:: 
    Make sure to use the same :math:`\eta`-function during interpolation as given by the joint :math:`\eta`-distribution passed to the constructor. 

.. Warning:: 
   The interpolation might lead to erroneous photon positions for clusters at the boarders of a frame. Make sure to filter out such cases.

.. Note:: 
    Make sure to use resonable energy bins, when constructing the joint distribution. If data is too sparse for a given energy the interpolation will lead to erreneous results.


.. py:currentmodule:: aare

.. autoclass:: Interpolator
    :special-members: __init__
    :members:
    :undoc-members:
    :inherited-members:

