# SPDX-License-Identifier: MPL-2.0
from . import _aare
import numpy as np
from .ScanParameters import ScanParameters

class RawFile(_aare.RawFile):
    """
    Class to read Raw files produced by slsDetectorPackage 

    Parameters:
        fname (str): Path to the master file.
        chunk_size (int, optional): Number of frames to read at a time. Defaults to 1.
        strixeltransform (list, optional): List of strixel transform objects. Defaults to None.
    """
    def __init__(self, fname, chunk_size = 1, strixeltransform : list = None):
        super().__init__(fname)
        self._chunk_size = chunk_size
        self._strixeltransform : list = strixeltransform
        if self._strixeltransform is not None:
            if(chunk_size != 1):
                raise ValueError(f"RawFile with strixeltransform must have chunk_size 1, given {chunk_size}") 
            rois = super().master.rois
            if(len(rois) != 1):
                raise ValueError(f"RawFile with strixeltransform must have exactly one ROI, found {len(rois)}") # TODO: for now only support one ROI 
            
            [transform.calculate_map(rois[0]) for transform in self._strixeltransform]

    def read(self) -> tuple:
        """Read the entire file.
        Seeks to the beginning of the file before reading.

        Returns:
            tuple: header, data
        """
        self.seek(0)
        return self.read_n(self.total_frames)

    def read_frame(self, frame_index: int | None = None ) -> tuple:
        """Read one frame from the file and then advance the file pointer.

        .. note::

            Uses the position of the file pointer :py:meth:`~RawFile.tell` to determine
            which frame to read unless frame_index is specified.

        Args:
            frame_index (int): If not None, seek to this frame before reading.

        Returns:
            tuple: header, data 
            if strixeltransform is None

            tuple: header, list[list] 
            if strixeltransform is not None, where the list contains for each strixeltransform a list of transformed data for each strixel group.

        Raises:
            RuntimeError: If the file is at the end.
        """
        if frame_index is not None:
            self.seek(frame_index)


        header, data = super().read_frame()
        if header.shape == (1,):
            header = header[0]

        if self._strixeltransform:
            res = [transform(data) for transform in self._strixeltransform]
            return header, res
        else:
            return header, data

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
        return self.total_frames

    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_value, traceback):
        pass

    def __iter__(self):
        return self
    
    def __next__(self):
        try:
            if self._chunk_size == 1:
                if self._strixeltransform is not None:
                    header, frame = self.read_frame()
                    return header, [transform(frame) for transform in self._strixeltransform]
                else:
                    return self.read_frame()
            else:
                return self.read_n(self._chunk_size)
            
                
        except RuntimeError:
            # TODO! find a good way to check that we actually have the right exception
            raise StopIteration
