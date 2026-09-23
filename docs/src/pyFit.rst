Fitting
-------

.. py:currentmodule:: aare

Aare fits one-dimensional scans and three-dimensional pixel data with
Minuit2. Create a model object and call its :meth:`fit` method::

    model = Gaussian(compute_errors=True)
    result = model.fit(x, y, y_err)

The model object is also callable, which evaluates it at the supplied points::

    fitted_y = model(x, result["par"])

The available models are ``Gaussian``, ``GaussianErfcPlateau``,
``GaussianChargeSharing``, ``GaussianChargeSharingKb``, ``Pol1``, ``Pol2``,
``RisingScurve``, and ``FallingScurve``. The module-level :func:`fit` function
accepts the same model objects when a functional interface is preferred.

For three-dimensional data, pass an array with shape
``(rows, columns, scan_points)`` and select the worker count with
``n_threads``::

    result = model.fit(x, image_data, image_errors, n_threads=8)

The result dictionary contains ``par`` and ``chi2``. It also contains
``par_err`` when ``compute_errors`` is enabled.

.. autofunction:: fit
