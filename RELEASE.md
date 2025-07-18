# Release notes


### head

Features:

- Cluster finder now works with 5x5, 7x7 and 9x9 clusters
- Added ClusterVector::empty() member
- Added apply_calibration function for Jungfrau data

Bugfixes:
- Fixed reading RawFiles with ROI fully excluding some sub files. 
- Decoding of MH02 files placed the pixels in wrong position
- Removed unused file: ClusterFile.cpp 


### 2025.05.22

Features:

- Added scurve fitting

Bugfixes:

- Fixed crash when opening raw files with large number of data files




