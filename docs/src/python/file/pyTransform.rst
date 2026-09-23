Transform
===================

The transform module takes data read by :class:`aare.CtbRawFile` and decodes it
to a useful image format. Depending on detector it supports both analog
and digital samples. 

For convenience the following transform objects are defined with a short name

.. code:: python

    moench05 = Moench05Transform()
    moench05_1g = Moench05Transform1g()
    moench05_old = Moench05TransformOld()
    matterhorn02 = Matterhorn02Transform()
    adc_sar_04_64to16 = AdcSar04Transform64to16()
    adc_sar_05_64to16 = AdcSar05Transform64to16()

.. py:currentmodule:: aare

.. automodule:: aare.transform
    :members:
    :undoc-members:
    :private-members:
    :special-members: __call__
    :show-inheritance:
    :inherited-members: