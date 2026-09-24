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

Choosing the minimizer
~~~~~~~~~~~~~~~~~~~~~~

Fits use Minuit2's Migrad by default. Fumili is a Gauss-Newton style
minimizer that takes the gradient and a linearised Hessian from the analytic
derivatives of the model, so it usually converges in far fewer function
evaluations. Select it with the ``minimizer`` argument or property::

    model = Gaussian(minimizer=Minimizer.Fumili)
    model.minimizer = Minimizer.Migrad

In the bundled ``fit_benchmark`` Fumili is about 1.5 times faster than Migrad
for Gaussian peaks with little noise and about three times faster for the
six-parameter S-curves. For very noisy data the Gauss-Newton steps converge
more slowly, so compare both minimizers on representative data.

With ``compute_errors`` enabled, Migrad reports parameter errors from Hesse
while Fumili reports them from the covariance of its linearised Hessian, so
Fumili does not run an extra Hesse step.

.. autofunction:: fit

.. autoclass:: Minimizer
    :members: Migrad, Fumili
