import numpy as np


arr = np.arange(10, dtype = np.int32)
np.save('test_1d_int32.npy', arr)

arr2 = np.zeros((3,2,5), dtype = np.float64)
arr2[0,0,0] = 1.0
arr2[0,0,1] = 2.0
arr2[0,1,0] = 72.0
arr2[2,0,4] = 63.0
np.save('test_3d_double.npy', arr2)