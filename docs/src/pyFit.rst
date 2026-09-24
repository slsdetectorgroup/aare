Fitting
-------

.. py:currentmodule:: aare

Aare fits one-dimensional scans and three-dimensional pixel data with a
built-in Levenberg-Marquardt solver that uses the analytic gradients of the
models. Create a model object and call its :meth:`fit` method::

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

Parameters and errors
^^^^^^^^^^^^^^^^^^^^^

Start values are estimated from the data. ``SetParameter`` overrides the
estimate, ``FixParameter`` keeps a parameter at a given value, and
``SetParLimits`` restricts it to a range where ``-inf`` and ``inf`` leave a
side open. The constructor arguments control the minimiser:

- ``max_calls`` caps the number of model evaluations per pixel. A pixel that
  does not converge within the budget returns zeros.
- ``tolerance`` is the EDM tolerance in Minuit's convention: the fit stops
  when the estimated distance to the minimum is below ``0.002 * tolerance``.
- ``strategy`` is accepted for compatibility and ignored.
- ``compute_errors`` returns Gauss-Newton parameter errors,
  ``sqrt(diag((J^T J)^-1))``. Fixed parameters and parameters that end on a
  limit report an error of 0.

Validation backend
^^^^^^^^^^^^^^^^^^

When aare is built from source with ``-DAARE_MINUIT2=ON``, the previous
Minuit2 (Migrad and Hesse) implementation is available as
``aare.experimental.fit_minuit2(model, x, y, y_err=None, n_threads=4)`` with
the same arguments and result as :func:`fit`. It exists to compare the two
solvers and is not part of the released packages.

.. autofunction:: fit
