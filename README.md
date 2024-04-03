# aare
Data analysis library for PSI hybrid detectors




## Folder structure 

| Folder   | subfolder     | Content                             |
|----------|---------------|-------------------------------------|
| include/ |  aare/        | top level header/s                  |
| core/    |    include/   | public headers for core             |
|          | src/          | source files and non public headers |

## file_io class diagram
![file_io class diagram](./extra/uml/out/file_io/ClassDiagram.png)



## Test the zmq socket with a detector simulator
 
**1. Download and build the slsDetectorPackage**

```bash
git clone https://github.com/slsdetectorgroup/slsDetectorPackage.git --branch=8.0.1 #or the desired branch
cd slsDetectorPackage
mkdir build && cd build
cmake .. -DSLS_USE_SIMULATOR=ON
make -j8 #or your number of cores
```

**2. Launch the slsReceiver**
```bash
bin/slsReceiver
```

**3. Launch the virtual server**
```bash
bin/jungfrauDetectorServer_virtual
```

**4 Configure the detector simulator**

```bash
#sample config file is in etc/ in the aare repo
sls_detector_put config etc/virtual_jf.config

#Now you can take images using sls_detector_acquire
sls_detector_acquire
```

**5. Run the zmq example**
```bash
examples/zmq_example

#Will print the headers fof the frames received

```


## Test the zmq processing replaying data

To be implemented