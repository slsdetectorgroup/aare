# Release notes

## head

### New Features:

- Expanding 24 to 32 bit data
- Decoding digital data from Mythen 302


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










