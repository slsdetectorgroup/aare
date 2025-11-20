# Release notes

### 2025.11.11

Fixes: 
- calculate_eta coincides with the theoretical definition

### 2025.10.29

Features:

- interpolator class supports using cross eta3x3 and eta3x3 on full cluster 
- interpolator class has option to calculate the rosenblatt transform 

Bugfixes: 

- eta interpolation 

### 2025.10.27

Features: 

- max_sum_2x2 including index of subcluster with highest energy is now available from Python API 
- eta stores corner as enum class cTopLeft, cTopRight, BottomLeft, cBottomRight indicating 2x2 subcluster with largest energy relative to cluster center 
- max_sum_2x2 returns corner as index
- interpolation can be used with eta3, cross eta3, eta2 and full eta2
- interpolation supports bilinear interpolation of eta values for more fine grained transformed uniform coordinates
- interpolation can use rosenblatt transform 
- interpolation is documented 

### 2025.10.1

Bugfixes: 

- File supports reading new master json file format (multiple ROI's not supported yet)
- Added tell to ClusterFile. Returns position in bytes for debugging

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







