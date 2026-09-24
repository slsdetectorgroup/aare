# SPDX-License-Identifier: MPL-2.0
import numpy as np
import pytest

import aare


def gaussian(x, A, mu, sigma):
    return A * np.exp(-0.5 * ((x - mu) / sigma) ** 2)


@pytest.fixture
def gaussian_data():
    x = np.linspace(-6.0, 6.0, 61)
    y = gaussian(x, 120.0, 0.8, 1.3)
    y_err = np.ones_like(y)
    return x, y, y_err


def test_gaussian_model_evaluates_and_fits_data():
    x = np.linspace(-5.0, 5.0, 51)
    expected = np.array([20.0, 0.5, 1.2])
    model = aare.Gaussian()

    y = model(x, expected)
    result = model.fit(x, y)

    np.testing.assert_allclose(result["par"], expected, atol=2e-3)


def test_fit_returns_errors_and_chi2(gaussian_data):
    x, y, y_err = gaussian_data
    model = aare.Gaussian(compute_errors=True)
    result = model.fit(x, y, y_err)

    assert set(result) == {"par", "par_err", "chi2"}
    assert result["par"].shape == (3,)
    assert result["par_err"].shape == (3,)
    assert result["chi2"].shape == (1,)
    np.testing.assert_allclose(result["par"], [120.0, 0.8, 1.3], rtol=1e-4)
    np.testing.assert_allclose(
        result["par_err"], [0.361, 0.00451, 0.00451], rtol=2e-2
    )
    assert result["chi2"][0] == pytest.approx(0.0, abs=1e-3)

    without_errors = aare.fit(aare.Gaussian(), x, y)
    assert set(without_errors) == {"par", "chi2"}


def test_pol1_weighted_errors_match_analytic_formula():
    x = np.arange(20, dtype=float)
    y = 2.0 + 0.5 * x
    y_err = 1.0 + 0.1 * x
    w = 1.0 / y_err**2
    S, Sx, Sxx = w.sum(), (w * x).sum(), (w * x * x).sum()
    delta = S * Sxx - Sx * Sx

    result = aare.Pol1(compute_errors=True).fit(x, y, y_err)

    np.testing.assert_allclose(result["par"], [2.0, 0.5], rtol=1e-6)
    np.testing.assert_allclose(
        result["par_err"],
        [np.sqrt(Sxx / delta), np.sqrt(S / delta)],
        rtol=1e-6,
    )


def test_fixed_parameter_reports_zero_error(gaussian_data):
    x, y, y_err = gaussian_data
    model = aare.Gaussian(compute_errors=True)
    model.FixParameter("sigma", 1.5)
    result = model.fit(x, y, y_err)

    assert result["par"][2] == 1.5
    assert result["par_err"][2] == 0.0
    assert result["par"][0] == pytest.approx(111.15, rel=1e-3)
    assert result["chi2"][0] == pytest.approx(1684.25, rel=1e-3)

    model.ReleaseParameter("sigma")
    result = model.fit(x, y, y_err)
    assert result["par"][2] == pytest.approx(1.3, rel=1e-4)


def test_active_limit_ends_on_the_limit(gaussian_data):
    x, y, _ = gaussian_data
    model = aare.Gaussian()
    model.SetParLimits("mu", -5.0, 0.5)
    result = model.fit(x, y)

    assert result["par"][1] == pytest.approx(0.5, abs=1e-9)
    assert result["par"][0] == pytest.approx(116.88, rel=1e-3)
    assert result["chi2"][0] == pytest.approx(4301.95, rel=1e-3)

    inactive = aare.Gaussian()
    inactive.SetParLimits(1, -5.0, 5.0)
    np.testing.assert_allclose(
        inactive.fit(x, y)["par"], [120.0, 0.8, 1.3], rtol=1e-4
    )


def test_user_start_value(gaussian_data):
    x, y, _ = gaussian_data
    model = aare.Gaussian()
    model.SetParameter("mu", 0.0)
    np.testing.assert_allclose(model.fit(x, y)["par"], [120.0, 0.8, 1.3], rtol=1e-4)


def test_max_calls_failure_returns_zeros(gaussian_data):
    x, y, y_err = gaussian_data
    result = aare.Gaussian(max_calls=3, compute_errors=True).fit(x, y, y_err)
    assert np.all(result["par"] == 0.0)
    assert np.all(result["par_err"] == 0.0)
    assert np.all(result["chi2"] == 0.0)


def test_strategy_is_accepted_and_ignored(gaussian_data):
    x, y, _ = gaussian_data
    a = aare.Gaussian(strategy=0).fit(x, y)
    b = aare.Gaussian(strategy=1).fit(x, y)
    np.testing.assert_array_equal(a["par"], b["par"])


def test_invalid_parameter_access_raises():
    model = aare.Gaussian()
    assert model.par_names == ["A", "mu", "sigma"]
    assert model.n_par == 3
    with pytest.raises(RuntimeError):
        model.SetParameter("nope", 1.0)
    with pytest.raises(IndexError):
        model.SetParameter(3, 1.0)
    with pytest.raises(RuntimeError):
        model.SetParLimits("mu", 1.0, 1.0)


@pytest.mark.parametrize("cls", [aare.RisingScurve, aare.FallingScurve])
def test_scurve_fit(cls):
    truth = np.array([10.0, 0.1, 50.0, 3.0, 1000.0, 2.0])
    x = np.linspace(0.0, 100.0, 100)
    model = cls(max_calls=500, compute_errors=True)
    y = model(x, truth)
    result = model.fit(x, y, np.ones_like(y))

    np.testing.assert_allclose(result["par"], truth, rtol=1e-3)
    assert np.all(result["par_err"] > 0.0)


@pytest.mark.parametrize("with_errors", [False, True])
def test_fit_3d_matches_fit_1d(with_errors):
    x = np.linspace(-6.0, 6.0, 61)
    rows, cols = 4, 3
    y = np.empty((rows, cols, x.size))
    for r in range(rows):
        for c in range(cols):
            y[r, c] = gaussian(x, 80.0 + 10.0 * r, -0.6 + 0.3 * c, 0.9 + 0.1 * r)
    y_err = np.ones_like(y) if with_errors else None

    model = aare.Gaussian(compute_errors=with_errors)
    cube = model.fit(x, y, y_err, n_threads=3)

    assert cube["par"].shape == (rows, cols, 3)
    assert cube["chi2"].shape == (rows, cols)
    for r in range(rows):
        for c in range(cols):
            single = model.fit(x, y[r, c], None if y_err is None else y_err[r, c])
            np.testing.assert_allclose(cube["par"][r, c], single["par"], rtol=1e-10)
            assert cube["chi2"][r, c] == pytest.approx(single["chi2"][0], abs=1e-10)
            if with_errors:
                np.testing.assert_allclose(
                    cube["par_err"][r, c], single["par_err"], rtol=1e-10
                )
    np.testing.assert_allclose(cube["par"][3, 2], [110.0, 0.0, 1.2], rtol=1e-4, atol=1e-6)


def test_minuit2_backend_matches_builtin_solver(gaussian_data):
    fit_minuit2 = getattr(aare.experimental, "fit_minuit2", None)
    if fit_minuit2 is None:
        pytest.skip("aare was built without AARE_MINUIT2")
    x, y, y_err = gaussian_data
    model = aare.Gaussian(compute_errors=True)
    model.SetParLimits("mu", -5.0, 0.5)
    lm = model.fit(x, y, y_err)
    mn = fit_minuit2(model, x, y, y_err)
    np.testing.assert_allclose(lm["par"], mn["par"], rtol=1e-3)
    np.testing.assert_allclose(lm["chi2"], mn["chi2"], rtol=1e-3)
