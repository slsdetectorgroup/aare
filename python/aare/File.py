import json
from typing import Any
import _aare
from . import Frame
import os


class File:
    """
    File class. uses proxy pattern to wrap around the pybinding class
    abstracts the python binding class that is requires type and detector information
    (e.g. _FileHandler_Jungfrau_16)
    """
    def __init__(self, path):
        """
        opens the master file and checks the dynamic range and detector
        
        """
        self.path = path
        # check if file exists
        if not os.path.exists(path):
            raise FileNotFoundError(f"File not found: {path}")
        ext = os.path.splitext(path)[1]

        if ext not in (".raw", ".json", ".npy"):
            raise ValueError(f"Invalid file extension: {ext}")
        
        if ext == ".json":
            # read the master file and get the detector and bitdepth
            master_data = json.load(open(path))
            detector = master_data["Detector Type"]
            bitdepth = None
            if 'Dynamic Range' not in master_data and detector == "Jungfrau":
                bitdepth = 16
            else:
                bitdepth = master_data["Dynamic Range"]
        elif ext == ".npy":
            # TODO: find solution for this. maybe add a None detector type
            detector = "Jungfrau"
            with open(path, "rb") as fobj:
                import numpy as np
                version = np.lib.format.read_magic(fobj)
                # find what function to call based on the version
                func_name = 'read_array_header_' + '_'.join(str(v) for v in version)
                func = getattr(np.lib.format, func_name)
                header = func(fobj)
                bitdepth = header[2].itemsize * 8
        else:
            NotImplementedError("Raw file not implemented yet")

        # class_name is of the form _FileHandler_Jungfrau_16...
        class_name = f"_FileHandler_{detector}_{bitdepth}"

        # this line is the equivalent of:
        # self._file = _FileHandler_Jungfrau_16(path)
        self._file = getattr(_aare, class_name)(path)
    
    def get_frame(self, frame_number):
        """
        returns a Frame object
        """
        c_frame= object.__getattribute__(self, "_file").get_frame(frame_number)
        return Frame(c_frame)

        
        
    def __getattribute__(self, __name: str) -> Any:
        """
        Proxy pattern to call the methods of the _file
        """
        if __name == "get_frame":
            return object.__getattribute__(self, "get_frame")
        
        return getattr(object.__getattribute__(self, "_file"), __name)





        