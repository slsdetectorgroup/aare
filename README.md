# aare
Data analysis library for PSI hybrid detectors


## Build and install

Prerequisites
- cmake >= 3.14
- C++17 compiler (gcc >= 8)
- python >= 3.10

### Development install (for Python)

```bash
git clone git@github.com:slsdetectorgroup/aare.git --branch=v1 #or using http...
mkdir build
cd build

#configure using cmake
cmake ../aare

#build (replace 4 with the number of threads you want to use)
make -j4 
```

Now you can use the Python module from your build directory

```python
import aare
f = aare.File('Some/File/I/Want_to_open_master_0.json')
```

To run form other folders either add the path to your conda environment using conda-build or add it to your PYTHONPATH


### Install using conda/mamba

```bash
#enable your env first!
conda install aare=2024.10.29.dev0 -c slsdetectorgroup
```

### Install to a custom location and use in your project

Working example in: https://github.com/slsdetectorgroup/aare-examples

```bash
#build and install aare 
git clone git@github.com:slsdetectorgroup/aare.git --branch=v1 #or using http...
mkdir build
cd build

#configure using cmake
cmake ../aare -DCMAKE_INSTALL_PREFIX=/where/to/put/aare

#build (replace 4 with the number of threads you want to use)
make -j4 

#install
make install


#Now configure your project
 cmake .. -DCMAKE_PREFIX_PATH=SOME_PATH
```

### Local build of conda pkgs

```bash
conda build . --variants="{python: [3.11, 3.12, 3.13]}"
```