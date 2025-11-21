# Release notes

This document describes the difference between Release 2025.8.22 and RELEASE_DATE. 

## Changes: 

### New Features: 

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










