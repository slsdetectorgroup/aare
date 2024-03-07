import json
from typing import Any
import _aare
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

        if ext not in (".raw", ".json"):
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
        else:
            NotImplementedError("Raw file not implemented yet")

        # class_name is of the form _FileHandler_Jungfrau_16...
        class_name = f"_FileHandler_{detector}_{bitdepth}"

        # this line is the equivalent of:
        # self._file = _FileHandler_Jungfrau_16(path)
        self._file = getattr(_aare, class_name)(path)
    

    def __getattribute__(self, __name: str) -> Any:
        """
        Proxy pattern to call the methods of the _file
        """
        return getattr(object.__getattribute__(self, "_file"), __name)





        