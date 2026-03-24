# Release notes

## 2026.3.17

### New Features:

- Decoding transceiver data from Matterhorn10 ``transformed_data = aare.transform.Matterhorn10Transform(num_counters=2, dynamic_range=16)(data)``
- Expanding 24 to 32 bit data ``aare._aare.expand24to32bit(data, offset=4)``
- Decoding digital data from Mythen 302 ``transformed_data = aare.transform.Mythen302Transform(offset=4)(data)``
- added ``aare.Interpolator.transform_eta_values``. Function transforms $`\eta`$-values to uniform spatial coordinates. Should only be used for easier debugging. 
- New ``to_string``, ``string_to`` 
- Added exptime and period members to RawMasterFile including decoding
- Removed redundant ``arr.value(ix,iy...)`` on NDArray use ``arr(ix,iy...)``
- Removed Print/Print_some/Print_all form NDArray (operator ``<<`` still works)
- Added const* version of .data()
- reading multiple ROI's supported for aare. 
    - Use ``aare.RawFile.read_roi(roi_index=0)`` to read a specific ROI for the current frame
    - Use ``aare.RawFile.read_rois()`` to read multiple ROIs for the current frame
    - Use ``aare.RawFile.read_n_with_roi(num_frames = 2, roi_index = 0)`` to read multiple frames for a specific ROI. 
    - Note ``read_frame`` and ``read_n`` is not supported for multiple ROI's. 
- Building conda/pypi pkgs for python 3.14. Removing 3.11 builds.

### Bugfixes: 

 - multi threaded cluster finder doesnt drop frames if queues are full 
 - Round before casting in the cluster finder to avoid biasing clusters by truncating


### 2025.11.21

### New Features: 

- Added SPDX-License-Identifier: MPL-2.0 to source files
- Calculate Eta3 supports all cluster types 
- interpolation class supports using cross eta3x3 and eta3x3 on full cluster as well as eta2x2 on full cluster
- interpolation class has option to calculate the rosenblatt transform 
- reduction operations to reduce Clusters of general size to 2x2 or 3x3 clusters 
- `max_sum_2x2` including index of subcluster with highest energy is now available from Python API 
- interpolation supports bilinear interpolation of eta values for more fine grained transformed uniform coordinates
- Interpolation is documented 

- Added tell to ClusterFile. Returns position in bytes for debugging

### Resolved Features: 

- calculate_eta coincides with theoretical definition

### Bugfixes: 

- eta calculation assumes correct photon center 
- eta transformation to uniform coordinates starts at 0
- Bug in interpolation 
- File supports reading new master json file format (multiple ROI's not supported yet)


### API Changes: 

- ClusterFinder for 2x2 Cluster disabled 
- eta stores corner as enum class cTopLeft, cTopRight, BottomLeft, cBottomRight indicating 2x2 subcluster with largest energy relative to cluster center 
- max_sum_2x2 returns corner as index 

### 2025.8.22

Features:

- Apply calibration works in G0 if passes a 2D calibration and pedestal
- count pixels that switch
- calculate pedestal (also g0 version)
- NDArray::view() needs an lvalue to reduce issues with the view outliving the array


Bugfixes:

- Now using glibc 2.17 in conda builds (was using the host)
- Fixed shifted pixels in clusters close to the edge of a frame

### 2025.7.18

Features:

- Cluster finder now works with 5x5, 7x7 and 9x9 clusters
- Added ClusterVector::empty() member
- Added apply_calibration function for Jungfrau data

Bugfixes:
- Fixed reading RawFiles with ROI fully excluding some sub files. 
- Decoding of MH02 files placed the pixels in wrong position
- Removed unused file: ClusterFile.cpp 


### 2025.5.22

Features:

- Added scurve fitting

Bugfixes:

- Fixed crash when opening raw files with large number of data files

## Download, Documentation & Support 

### Download

The Source Code: 
https://github.com/slsdetectorgroup/aare


### Documentation 


Documentation including installation details: 
https://github.com/slsdetectorgroup/aare 


### Support


erik.frojdh@psi.ch \
alice.mazzoleni@psi.ch \
dhanya.thattil@psi.ch










