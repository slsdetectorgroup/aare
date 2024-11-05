
from . import _aare
import numpy as np

class CtbRawFile(_aare.CtbRawFile):
    def __init__(self, fname, transform = None):
        super().__init__(fname)
        self.transform = transform


    def read_frame(self, frame_index = None):
        """Read one frame from the file.

        Args:
            frame_index (int): If not None, seek to this frame before reading.

        Returns:
            tuple: header, data
        """
        if frame_index is not None:
            self.seek(frame_index)

        header, data = super().read_frame()
        if header.shape == (1,):
            header = header[0]


        if self.transform:
            res = self.transform(data)
            if isinstance(res, tuple):
                return header, *res
            else:
                return header, res
            
    def read_n(self, n_frames):
        # Do the first read to figure out what we have
        tmp_header, tmp_data = self.read_frame()
        
        # Allocate arrays for
        header = np.zeros(n_frames, dtype = tmp_header.dtype)
        data = np.zeros((n_frames, *tmp_data.shape), dtype = tmp_data.dtype)
        
        # Copy the first frame
        header[0] = tmp_header
        data[0] = tmp_data

        # Do the rest of the reading
        for i in range(1, n_frames):
            header[i], data[i] = self.read_frame()

        return header, data


    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_value, traceback):
        pass

    def __iter__(self):
        return self
    
    def __next__(self):
        try:
            return self.read_frame()
        except RuntimeError:
            # TODO! find a good way to check that we actually have the right exception
            raise StopIteration

