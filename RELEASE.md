# Release notes


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




