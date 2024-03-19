import os
from pathlib import Path
from aare import File, Frame, DataSpan, ImageData
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
    
    span = DataSpan(frame)
    image = ImageData(frame)
    def f(a,b,c, cord):
        print(f"Frame: {a.get(*cord)} Span: {b.get(*cord)} Image: {c.get(*cord)}")
    
    f(frame, span, image, (0,0))
    frame.set(0,0, 100)
    f(frame, span, image, (0,0))
