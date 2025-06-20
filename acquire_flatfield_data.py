from slsdet import Mythen3,timingMode,detectorSettings,runStatus,dacIndex,scanParameters
#from patterntools.zmqreceiver import ZmqReceiver
#from detConf_module import *
import numpy as np
from slsdet.lookup import view, find
from epics import caput, caget
import time


d = Mythen3()
#d.detsize = [10, 1]

d.hostname = 'localhost:2000+localhost:2002'

d.rx_hostname = 'localhost' 

#d.rx_tcpport = 2012

d.udp_dstport = 5006 

d.udp_dstip = 'auto'

d.udp_srcip = 'auto' 

#rx=makeReceiver(d) 

nmod=len(d.hostname)
print("num modules: ", nmod)

d.frames = 3
d.exptime = 5

d.fwrite=1
#d.counters = [0, 1]
print(d.counters)

d.fpath = "~/Documents/tmp/Flatfieldacquisition"
d.fwrite = 1

#where do i set the strips? 
#d.detsize = 2 do I have to set this

d.startReceiver()
d.startDetector() 
for angle in [87,2]:
    d.fname = 'run_'+str(angle)
    caput('BL11I-MO-DIFF-01:DELTA.VAL',angle,wait=False)
    print("moving detector to ",angle)
    d.acquire()
    time.sleep(d.exptime)
    while d.status != runStatus.IDLE:
        time.sleep(0.01)
            
    nf=np.min(d.rx_framescaught)
    ang=caget('BL11I-MO-DIFF-01:DELTA.RBV')
    print("angle is: ", ang)
    print("caught frames: ", nf)
        
d.stopReceiver()

#dd, hh = rx[imod].receive_one_frame() what is this - does this exist in cpp? 



