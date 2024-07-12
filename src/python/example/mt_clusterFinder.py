from pathlib import Path
import sys
PROJECT_ROOT_DIR=(Path(__file__) / "../../../../").resolve()
print(PROJECT_ROOT_DIR)
sys.path.append(str((PROJECT_ROOT_DIR / 'build').resolve()))
from threading import Thread

from _aare import *

file_path = None
N_THREADS = None
if len(sys.argv) <= 2:
    raise Exception("Usage: mt_clusterFinder.py <file> <n_threads>")
else:
    file_path = sys.argv[1]
    N_THREADS = int(sys.argv[2])

file = File(file_path)
pedestal=Pedestal(400,400,1000)
for i in range(1000):
    frame = file.iread(i)
    pedestal.push(frame)
print("Pedestal done")

def f(idx,n):
    def g():
        print("Hello from thread",idx)
        f = File(file_path)
        p = pedestal.copy()
        cf = ClusterFinder(3,3,5.0,0)
        for i in range(idx,1000,n):
            frame = f.iread(i)
            clusters=cf.find_clusters_without_threshold(frame,p,False)

        print("Goodbye from thread",idx)
    return g
    

mt = MultiThread([f(i,N_THREADS) for i in range(N_THREADS)])
mt.run()