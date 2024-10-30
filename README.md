# aare
Data analysis library for PSI hybrid detectors



## Development install (for Python)

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


## Project structure 

include/aare - public headers


## Open questions

- How many sub libraries? 
- Where to place test data? This data is also needed for github actions...
- What to return to numpy? Our NDArray or a numpy ndarray? Lifetime? 