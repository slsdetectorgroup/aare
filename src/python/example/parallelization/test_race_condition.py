"""
    try to trigger race condition errors and find out where they happen.
    Three examples are implemented:
    1. 4 threads changing the same array in sync
    2. 4 threads changing the same array in parallel
    3. 1 thread changing the array 4 times

    the array 

"""


from pathlib import Path
import sys
import numpy as np
PROJECT_ROOT_DIR=Path("../../../../").resolve()
sys.path.append(str((PROJECT_ROOT_DIR / 'build').resolve()))

from _aare import *

arr1 = np.random.randint(1000,size=(400,400)).astype(np.int32)
print("initial:")
print(arr1)
print()
arr2 = arr1.copy()
arr3 = arr1.copy()

N_TRIALS = 2000
N_THREADS = 4

def f():
    print("Hello from thread SYNC")
    test_race_condition(arr1,False,N_TRIALS)
    print("Goodbye from thread SYNC")

def g():
    print("Hello from thread PARALLEL")
    test_race_condition(arr2,True,N_TRIALS)
    print("Goodbye from thread PARALLEL")
# 
mt = MultiThread([f for i in range(N_THREADS)])
mt.run()
# print("gil acquired:")
# print(arr1)
# print()


mt = MultiThread([g for i in range(N_THREADS)])
mt.run()
# print("gil released:")
# print(arr2)
# print()



for trials in (range(N_TRIALS*4)):
    arr3= np.where(arr3%2==0,arr3*5+1,arr3-3)

# print("without threads:")
# print(arr3)
# print()

print()
print("RESULTS:")
print("PARALLEL==SYNC",np.array_equal(arr1,arr2))
print("how many differents?",np.sum(arr1!=arr2))
print()
print("PARALLEL==NO THREADS",np.array_equal(arr2,arr3))
print("how many differents?",np.sum(arr2!=arr3))
print()
print("SYNC==NO THREADS",np.array_equal(arr1,arr3))
print("how many differents?",np.sum(arr1!=arr3))

# print("without threads:")
# print(arr3)
# print()

# print("arr1==arr3",np.array_equal(arr1,arr2))
# print("arr2==arr3",np.array_equal(arr2,arr3))


