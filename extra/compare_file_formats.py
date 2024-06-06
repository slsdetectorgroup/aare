import numpy as np
import time
def timing_val(f):
    def wrapper(*arg, **kw):
        t1 = time.time()
        res = f(*arg, **kw)
        t2 = time.time()
        return (t2 - t1), res, f.__name__
    return wrapper

N_CLUSTERS = 1,000,000

"""
fixed size clusters:
header: 
    - magic_string: 4 bytes
    - version: 1 byte
    - n_records: 4 bytes
    - indexed: 1 byte
    - metadata_length: 1 byte (number of chars)
    - metadata: metadata_length bytes (json string)
    - field_count: 1 byte
    - fields:
        - field_label_length: 1 byte
        - field_label: field_label_length bytes (string)
        - dtype: 3 bytes (string)
        - is_array: 1 byte (0: not array, 1:fixed_length_array, 2:variable_length_array)
        - array_length: 4 bytes (if is_array == 1) 

data:
    - field: (field_1_dtype_length bytes) or 
    
"""

def binary_clusters():
    with open('fixed_size_clusters.bin', 'wb') as f:
        for i in range(N_CLUSTERS):

            