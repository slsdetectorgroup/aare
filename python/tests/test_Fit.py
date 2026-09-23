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
