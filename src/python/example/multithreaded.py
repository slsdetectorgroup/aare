from pathlib import Path
import sys
PROJECT_ROOT_DIR=Path("../../../").resolve()
sys.path.append(str((PROJECT_ROOT_DIR / 'build').resolve()))

from _aare import *

def f(idx,n):
    def g():
        print("Hello from thread",idx)
        file = File("/home/l_bechir/tmp/testNewFW20230714/cu_half_speed_master_4.json")
        p=Pedestal(400,400,1000)
        for i in range(5000):
            frame = file.iread(i)
            p.push(frame)
        print("Goodbye from thread",idx)
    return g
    
mt = MultiThread([f(i,4) for i in range(4)])
mt.run()

