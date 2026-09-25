Fitting
-------

.. py:currentmodule:: aare

Aare fits one-dimensional scans and three-dimensional pixel data with
Minuit2 or a built-in Levenberg-Marquardt solver. Create a model object and
call its :meth:`fit` method::

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

.. note::

    The fit works on ``float64`` data. If ``x``, ``y`` or ``y_err`` is not
    already a C-contiguous ``float64`` array, the Python bindings convert it
    before fitting, which allocates a full copy. Integer or ``float32``
    input therefore works, but for a large three-dimensional ``y`` the
    copy can be several times the size of the original array. Pass
    ``float64`` arrays to avoid it. A ``float64`` C-contiguous array is used
    in place without copying.

Choosing the minimizer
~~~~~~~~~~~~~~~~~~~~~~

Fits use Minuit2's Migrad by default. Three alternatives share the same
interface and are selected with the ``minimizer`` argument or property::

    model = Gaussian(minimizer=Minimizer.LevenbergMarquardt)
    model.minimizer = Minimizer.Fumili

- ``Minimizer.Migrad``: Minuit2's variable-metric minimizer, driven by the
  analytic gradient of the chi-squared. Parameter errors come from Hesse.
- ``Minimizer.Fumili``: Minuit2's Gauss-Newton minimizer with a trust region.
  It takes the gradient and a linearised Hessian from the analytic derivatives
  of the model and usually needs far fewer function evaluations than Migrad.
  Parameter errors come from the covariance of the linearised Hessian, without
  an extra Hesse step. Minuit2's Fumili cannot converge once a two-sided limit
  becomes active; a pixel without a valid Fumili minimum is refitted with
  Migrad.
- ``Minimizer.LevenbergMarquardt``: a built-in damped Gauss-Newton solver with
  the analytic Jacobian of the model. It reflects steps at parameter limits
  and reuses its buffers between pixels, so data cubes are fitted without
  per-pixel allocations. Parameter errors are ``sqrt(diag((J^T J)^-1))``.
- ``Minimizer.VarPro``: a built-in variable projection solver.
  Every bundled model is linear in some of its parameters (the amplitude of a
  Gaussian, the baseline and amplitude of an S-curve, every parameter of a
  polynomial). Each trial point solves those exactly and the
  Levenberg-Marquardt iteration runs over the remaining nonlinear parameters
  only, so it needs fewer evaluations than LevenbergMarquardt and cannot be
  misled by poor start values of the linear parameters, which it ignores.
  A pixel whose linear solution violates a limit on a linear parameter, or
  that does not converge, is refitted with LevenbergMarquardt. Parameter
  errors are the same Gauss-Newton estimates.

All four converge to the same minimum on well-behaved data; the ``[fit]``
test suite checks that. The bundled ``fit_benchmark`` compares their speed on
Gaussian peaks, S-curves and a data cube. For very noisy data the Gauss-Newton
steps of Fumili and LevenbergMarquardt converge more slowly, so compare the
minimizers on representative data.

Parameters and errors
~~~~~~~~~~~~~~~~~~~~~

Start values are estimated from the data. ``SetParameter`` overrides the
estimate, ``FixParameter`` keeps a parameter at a given value, and
``SetParLimits`` restricts it to a range where ``-inf`` and ``inf`` leave a
side open (the Minuit2 minimizers turn an open side into a wide finite one);
free start values are clamped into their limits. The constructor arguments
control the minimizer:

- ``max_calls`` caps the work per pixel: Minuit2 function calls for Migrad and
  Fumili, model evaluations for LevenbergMarquardt and VarPro,
  which spend one evaluation per iteration (two when a step crosses a limit).
  ``0`` selects Minuit's default budget. A pixel that does not converge within
  the budget returns zeros.
- ``tolerance`` is the EDM tolerance in Minuit's convention. Migrad,
  LevenbergMarquardt and VarPro stop when the estimated distance
  to the minimum is below ``0.002 * tolerance``, Fumili below
  ``1e-4 * tolerance``.
- ``strategy`` is the Minuit2 strategy (0 is fast, 1 is Minuit2's default).
  LevenbergMarquardt and VarPro ignore it.
- ``compute_errors`` adds ``par_err`` to the result. Fixed parameters and
  parameters that end on a limit report an error of 0 with every minimizer.

Unknown parameter names raise ``RuntimeError``, bad indices ``IndexError``,
and ``SetParLimits`` rejects a lower limit at or above the upper one.

.. autofunction:: fit

.. autoclass:: Minimizer
    :members: Migrad, Fumili, LevenbergMarquardt, VarPro
