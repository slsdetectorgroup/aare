# SPDX-License-Identifier: MPL-2.0
import numpy as np
import aare



def test_gaussian_model_evaluates_and_fits_data():
    x = np.linspace(-5.0, 5.0, 51)
    expected = np.array([20.0, 0.5, 1.2])
    model = aare.Gaussian()

    y = model(x, expected)
    result = model.fit(x, y)

    np.testing.assert_allclose(result["par"], expected, atol=2e-3)


def test_minimizer_can_be_selected_and_fumili_fits_data():
    x = np.linspace(-5.0, 5.0, 51)
    expected = np.array([20.0, 0.5, 1.2])

    assert aare.Gaussian().minimizer == aare.Minimizer.Migrad

    model = aare.Gaussian(minimizer=aare.Minimizer.Fumili)
    assert model.minimizer == aare.Minimizer.Fumili

    y = model(x, expected)
    result = model.fit(x, y)
    np.testing.assert_allclose(result["par"], expected, atol=2e-3)

    model.minimizer = aare.Minimizer.Migrad
    assert model.minimizer == aare.Minimizer.Migrad


def test_fumili_and_migrad_agree_on_3d_data_with_errors():
    x = np.linspace(-5.0, 5.0, 51)
    expected = np.array([20.0, 0.5, 1.2])
    single = aare.Gaussian()(x, expected)
    y = np.tile(single, (2, 3, 1))
    y_err = np.ones_like(y)

    migrad = aare.Gaussian(compute_errors=True)
    fumili = aare.Gaussian(compute_errors=True, minimizer=aare.Minimizer.Fumili)

    with_migrad = migrad.fit(x, y, y_err, n_threads=2)
    with_fumili = fumili.fit(x, y, y_err, n_threads=2)

    assert with_fumili["par"].shape == (2, 3, 3)
    assert with_fumili["par_err"].shape == (2, 3, 3)
    for result in (with_migrad, with_fumili):
        np.testing.assert_allclose(result["par"], np.broadcast_to(expected, (2, 3, 3)), atol=2e-3)
        np.testing.assert_allclose(result["chi2"], 0.0, atol=1e-3)
    np.testing.assert_allclose(with_fumili["par_err"], with_migrad["par_err"], rtol=2e-2)
