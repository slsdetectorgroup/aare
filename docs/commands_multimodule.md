# receive data from two zmqstreams
```bash
killall jungfrauDetectorServer_virtual
jungfrauDetectorServer_virtual 
jungfrauDetectorServer_virtual -p 1956
slsMultiReceiver 1980 2 0
sls_detector_put config etc/multimodule_virtual_jf.config
```
