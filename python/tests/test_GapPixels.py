# SPDX-License-Identifier: MPL-2.0
import numpy as np
import pytest

from aare import DetectorType, GapPixels, ModuleGaps, ROI, gapped_shape, insert_gap_pixels


def reference(image, boundaries_x, boundaries_y, fill=0):
    """Insert gap columns and rows with numpy: boundaries map a source index to
    the number of pixels inserted before it."""
    cols = [index for index, count in boundaries_x for _ in range(count)]
    rows = [index for index, count in boundaries_y for _ in range(count)]
    result = np.insert(image, cols, fill, axis=1)
    return np.insert(result, rows, fill, axis=0)


def test_configuration_defaults_and_equality():
    gaps = GapPixels()
    assert gaps.chip_gap == 2
    assert gaps.module_gaps is None
    assert gaps.fill_value == 0
    assert gaps.split_counts is False
    assert gaps.seed is None
    assert gaps == GapPixels()
    assert gaps != GapPixels(fill_value=1)

    gaps = GapPixels(chip_gap=4, module_gaps=ModuleGaps(8, 36), fill_value=-1, split_counts=False, seed=5)
    assert gaps.module_gaps == ModuleGaps(x=8, y=36)
    assert gaps.seed == 5
    assert "ModuleGaps(x=8, y=36)" in repr(gaps)
    gaps.module_gaps = None
    assert gaps.module_gaps is None


def test_gapped_shape():
    assert gapped_shape(ROI(0, 1024, 0, 512), DetectorType.Jungfrau) == (514, 1030)
    assert gapped_shape(ROI(0, 1024, 0, 1024), DetectorType.Jungfrau) == (1028, 1030)
    assert gapped_shape(ROI(0, 1024, 0, 1024), DetectorType.Jungfrau, GapPixels(module_gaps=ModuleGaps(8, 36))) == (1064, 1030)
    assert gapped_shape(ROI(0, 512, 0, 512), DetectorType.Eiger, quad=True) == (514, 514)
    assert gapped_shape(ROI(256, 512, 0, 256), DetectorType.Jungfrau) == (256, 256)
    assert gapped_shape(ROI(255, 257, 0, 1), DetectorType.Jungfrau) == (1, 4)
    with pytest.raises(ValueError):
        gapped_shape(ROI(0, 400, 0, 400), DetectorType.Moench)


@pytest.mark.parametrize("dtype", [np.uint8, np.uint16, np.uint32, np.int32, np.float32, np.float64])
def test_insert_gap_pixels_matches_numpy(dtype):
    rng = np.random.default_rng(1)
    image = rng.integers(0, 200, size=(512, 1024)).astype(dtype)
    result = insert_gap_pixels(image, DetectorType.Jungfrau, GapPixels(fill_value=3))
    expected = reference(image, [(256, 2), (512, 2), (768, 2)], [(256, 2)], fill=3)
    assert result.dtype == image.dtype
    assert result.shape == (514, 1030)
    assert (result == expected).all()


def test_insert_gap_pixels_without_module_gaps():
    # two stacked modules: chip gaps at rows 256 and 768, none at the module
    # boundary at row 512
    image = np.arange(1024 * 1024, dtype=np.uint32).reshape(1024, 1024)
    result = insert_gap_pixels(image, DetectorType.Jungfrau)
    expected = reference(image, [(256, 2), (512, 2), (768, 2)], [(256, 2), (768, 2)])
    assert result.shape == (1028, 1030)
    assert (result == expected).all()


def test_insert_gap_pixels_with_module_gaps():
    image = np.arange(1024 * 1024, dtype=np.uint32).reshape(1024, 1024)
    gaps = GapPixels(module_gaps=ModuleGaps(8, 36), fill_value=9)
    result = insert_gap_pixels(image, DetectorType.Jungfrau, gaps)
    expected = reference(image, [(256, 2), (512, 2), (768, 2)], [(256, 2), (512, 36), (768, 2)], fill=9)
    assert result.shape == (1064, 1030)
    assert (result == expected).all()


def test_insert_gap_pixels_in_roi():
    image = np.array([[11, 22]], dtype=np.uint16)
    result = insert_gap_pixels(image, DetectorType.Jungfrau, roi=ROI(255, 257, 10, 11))
    assert result.tolist() == [[11, 0, 0, 22]]

    # a gap on the ROI border is not inserted
    image = np.arange(8, dtype=np.uint16).reshape(2, 4)
    result = insert_gap_pixels(image, DetectorType.Jungfrau, roi=ROI(256, 260, 0, 2))
    assert (result == image).all()


def test_insert_gap_pixels_split_counts():
    rng = np.random.default_rng(2)
    image = rng.integers(0, 1000, size=(512, 1024)).astype(np.uint32)
    gaps = GapPixels(split_counts=True, seed=3)
    first = insert_gap_pixels(image, DetectorType.Jungfrau, gaps)
    second = insert_gap_pixels(image, DetectorType.Jungfrau, gaps)
    assert first.sum() == image.sum()
    assert (first == second).all()
    # edge pixel and its gap pixel hold floor and ceil of the half; row 255
    # is excluded because it is split again vertically
    pair = np.stack([first[:255, 255], first[:255, 256]])
    assert (pair.sum(axis=0) == image[:255, 255]).all()
    assert (pair.max(axis=0) == (image[:255, 255] + 1) // 2).all()

    halves = insert_gap_pixels(np.full((2, 2), 3.0), DetectorType.Jungfrau, gaps, roi=ROI(255, 257, 255, 257))
    assert (halves == 0.75).all()


def test_insert_gap_pixels_rejects_bad_input():
    image = np.zeros((400, 400), dtype=np.uint16)
    with pytest.raises(ValueError):
        insert_gap_pixels(image, DetectorType.Moench)
    with pytest.raises(ValueError):
        insert_gap_pixels(image, DetectorType.Jungfrau, roi=ROI(0, 401, 0, 400))
    with pytest.raises(ValueError):
        insert_gap_pixels(np.zeros((2, 2, 2), dtype=np.uint16), DetectorType.Jungfrau)
    with pytest.raises(TypeError):
        insert_gap_pixels(np.zeros((2, 2), dtype=np.int64), DetectorType.Jungfrau)
    with pytest.raises(ValueError):
        insert_gap_pixels(image, DetectorType.Jungfrau, GapPixels(split_counts=True, chip_gap=4))
