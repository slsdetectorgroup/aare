import os
from pathlib import Path
from aare import File, Frame

if __name__ == "__main__":
    #get env variable
    root_dir = Path(os.environ.get("PROJECT_ROOT_DIR"))
    data_path = str(root_dir / "data"/"jungfrau_single_master_0.json")

    file = File(data_path)
    frame = file.get_frame(0)
    print(frame.rows, frame.cols)
    print(frame.get(0,0))