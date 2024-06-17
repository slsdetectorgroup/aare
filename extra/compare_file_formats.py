import numpy as np
import zipfile
from zipfile import ZipFile as zf
import time
from pathlib import Path
import sqlite3


def timing_val(f):
    def wrapper(*arg, **kw):
        t1 = time.time()
        res = f(*arg, **kw)
        t2 = time.time()
        return (t2 - t1), res, f.__name__

    return wrapper


N_CLUSTERS = 1_000_000

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
        - array_length: 4 bytes (used if is_array == 1) 

data:
    - field: (field_1_dtype_length bytes) or 
    
"""


header_length = 4 + 1 + 4 + 1 + 1 + 2 + 1 + (1 + 5 + 3 + 1 + 4) * 3
cluster_dt = np.dtype([("x", "int16"), ("y", "int16"), ("data", "int32", (3, 3))])


def write_binary_clusters():
    with open("fixed_size_clusters.bin", "wb") as f:
        f.write(b"H" * header_length)
        arr = np.zeros(N_CLUSTERS, dtype=cluster_dt)
        f.write(arr.tobytes())


def write_numpy_clusters():
    np.save("numpy_clusters.npy", np.zeros(N_CLUSTERS, dtype=cluster_dt))


def write_sqlite_clusters():
    data = np.zeros(9, dtype=np.int32).tobytes()
    c = conn.cursor()
    c.execute("CREATE TABLE clusters (x int, y int, data blob)")
    c.executemany("INSERT INTO clusters VALUES (?, ?, ?)", [(0, 0, data)] * N_CLUSTERS)
    conn.commit()


READ_N_CLUSTERS = N_CLUSTERS


def read_binary_clusters():
    with open("fixed_size_clusters.bin", "rb") as f:
        f.read(header_length)
        f.read(READ_N_CLUSTERS * cluster_dt.itemsize)


def read_numpy_clusters():
    arr = np.load("numpy_clusters.npy")


def read_sqlite_clusters():
    c = conn.cursor()
    c.execute("SELECT * FROM clusters LIMIT ?", (READ_N_CLUSTERS,))
    arr = c.fetchall()


N_APPEND_CLUSTERS = 100_000


def append_binary_clusters():
    with open("fixed_size_clusters.bin", "ab") as f:
        arr = np.zeros(N_APPEND_CLUSTERS, dtype=cluster_dt)
        f.write(arr.tobytes())


def append_sqlite_clusters():
    data = np.zeros(9, dtype=np.int32).tobytes()
    c = conn.cursor()
    c.executemany(
        "INSERT INTO clusters VALUES (?, ?, ?)", [(0, 0, data)] * N_APPEND_CLUSTERS
    )
    conn.commit()


def p(write_time, file_size):
    file_size = file_size / 1024 / 1024
    print("%.3fs" % write_time, "%.3fMB" % file_size)


if __name__ == "__main__":
    # setup
    Path("fixed_size_clusters.bin").unlink(missing_ok=True)
    Path("numpy_clusters.npy").unlink(missing_ok=True)
    Path("sqlite_clusters.db").unlink(missing_ok=True)
    Path("fixed_size_clusters.zip").unlink(missing_ok=True)
    Path("numpy_clusters.zip").unlink(missing_ok=True)
    Path("sqlite_clusters.zip").unlink(missing_ok=True)
    conn = sqlite3.connect("sqlite_clusters.db")

    # run
    print("Testing file creation", f"(N_CLUSTERS={N_CLUSTERS}):")
    print("Binary clusters:", end=" ")
    bin_time, _, _ = timing_val(write_binary_clusters)()
    bin_size = Path("fixed_size_clusters.bin").stat().st_size
    p(bin_time, bin_size)

    print("Numpy clusters:", end=" ")
    np_time, _, _ = timing_val(write_numpy_clusters)()
    np_size = Path("numpy_clusters.npy").stat().st_size
    p(np_time, np_size)

    print("SQLite clusters:", end=" ")
    sql_time, _, _ = timing_val(write_sqlite_clusters)()
    sql_size = Path("sqlite_clusters.db").stat().st_size
    p(sql_time, sql_size)

    print("\nTesting file reading", f"(READ_N_CLUSTERS={READ_N_CLUSTERS}):")
    print("Binary clusters:", end=" ")
    print("%.5fs" % timing_val(read_binary_clusters)()[0])
    print("Numpy clusters:", end=" ")
    print("%.5fs" % timing_val(read_numpy_clusters)()[0])
    print("SQLite clusters:", end=" ")
    print("%.5fs" % timing_val(read_sqlite_clusters)()[0])

    print("\nTesting appending to file:")
    print("Binary clusters:", end=" ")
    print("%.5fs" % timing_val(append_binary_clusters)()[0])
    print("SQLite clusters:", end=" ")
    print("%.5fs" % timing_val(append_sqlite_clusters)()[0])

    print("\nTesting zip compression:")
    print("Binary clusters compressed:", end=" ")
    with zf("fixed_size_clusters.zip", "w", zipfile.ZIP_DEFLATED) as z:
        z.write("fixed_size_clusters.bin")
    print(
        "%.3fMB" % (Path("fixed_size_clusters.zip").stat().st_size / 1024 / 1024),
        end=" ",
    )
    rate = (1 - Path("fixed_size_clusters.zip").stat().st_size / bin_size) * 100
    print("rate:", "%.2f" % rate + "%")
    print("Numpy clusters compressed:", end=" ")
    with zf("numpy_clusters.zip", "w", zipfile.ZIP_DEFLATED) as z:
        z.write("numpy_clusters.npy")
    print("%.3fMB" % (Path("numpy_clusters.zip").stat().st_size / 1024 / 1024), end=" ")
    rate = (1 - Path("numpy_clusters.zip").stat().st_size / bin_size) * 100
    print("rate:", "%.2f" % rate + "%")
    print("SQLite clusters compressed:", end=" ")
    with zf("sqlite_clusters.zip", "w", zipfile.ZIP_DEFLATED) as z:
        z.write("sqlite_clusters.db")
    print(
        "%.3fMB" % (Path("sqlite_clusters.zip").stat().st_size / 1024 / 1024), end=" "
    )
    rate = (1 - Path("sqlite_clusters.zip").stat().st_size / bin_size) * 100
    print("rate:", "%.2f" % rate + "%")

    # clean
    conn.close()
    # Path("fixed_size_clusters.bin").unlink(missing_ok=True)
    # Path("numpy_clusters.npy").unlink(missing_ok=True)
    # Path("sqlite_clusters.db").unlink(missing_ok=True)
