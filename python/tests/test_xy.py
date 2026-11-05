# SPDX-License-Identifier: MPL-2.0
from aare import xy


def test_xy_fields():
    layout = xy(row=2, col=3)
    assert layout.row == 2
    assert layout.col == 3

    layout.row = 4
    assert layout.row == 4


def test_xy_unpacks_as_row_col():
    row, col = xy(2, 3)
    assert (row, col) == (2, 3)


def test_xy_comparison():
    assert xy(2, 3) == xy(2, 3)
    assert xy(2, 3) != xy(3, 2)
    assert xy(2, 3) != (2, 3)


def test_xy_repr():
    assert repr(xy(2, 3)) == "xy(row=2, col=3)"
