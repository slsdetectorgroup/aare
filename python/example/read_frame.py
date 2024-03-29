import os
from pathlib import Path
from aare import File, Frame
import numpy as np

if __name__ == "__main__":


    #get env variable
    root_dir = Path(os.environ.get("PROJECT_ROOT_DIR"))

    # read JSON master file
    data_path = str(root_dir / "data"/"jungfrau_single_master_0.json")

    file = File(data_path)
    frame = file.get_frame(0)
    print(frame.rows, frame.cols)
    print(frame.get(0,0))

    # read Numpy file

    data_path = str(root_dir / "data"/"test_numpy_file.npy")
    file = File(data_path)
    frame = file.get_frame(0)
    print(frame.rows, frame.cols)
    print(frame.get(0,0))
    
    arr = np.array(frame.get_array())
    print(arr)
    print(arr.shape)
    
    print(np.array_equal(arr, np.load(data_path)[0]))