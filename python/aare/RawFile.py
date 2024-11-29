from . import _aare
import numpy as np
from .ScanParameters import ScanParameters

class RawFile(_aare.RawFile):
    def __init__(self, fname, chunk_size = 1):
        super().__init__(fname)
        self._chunk_size = chunk_size


    def read(self) -> tuple:
        """Read the entire file.
        Seeks to the beginning of the file before reading.

        Returns:
            tuple: header, data
        """
        self.seek(0)
        return self.read_n(self.total_frames)

    @property
    def scan_parameters(self):
        """Return the scan parameters.

        Returns:
            ScanParameters: Scan parameters.
        """
        return ScanParameters(self.master.scan_parameters)
    
    @property
    def master(self):
        """Return the master file.

        Returns:
            RawMasterFile: Master file.
        """
        return super().master
    
    def __len__(self) -> int:
        """Return the number of frames in the file.

        Returns:
            int: Number of frames in file.
        """
        return super().frames_in_file

    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_value, traceback):
        pass

    def __iter__(self):
        return self
    
    def __next__(self):
        try:
            if self._chunk_size == 1:
                return self.read_frame()
            else:
                return self.read_n(self._chunk_size)
            
                
        except RuntimeError:
            # TODO! find a good way to check that we actually have the right exception
            raise StopIteration
