
CtbRawFile
============

Read analog, digital and transceiver samples from a raw file containing
data from the Chip Test Board. Uses :mod:`aare.transform` to decode the 
data into a format that the user can work with. 

.. code:: python

    import aare
    from aare.transform import Mythen302Transform
    my302 = Mythen302Transform(offset = 4)

    with aare.CtbRawFile(fname, transform = my302) as f:
    for header, data in f:
       #do something with the data

.. py:currentmodule:: aare

.. autoclass:: CtbRawFile
    :members:
    :undoc-members:
    :show-inheritance:
    :inherited-members: