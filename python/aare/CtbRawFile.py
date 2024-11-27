
from . import _aare
import numpy as np
from .ScanParameters import ScanParameters

class CtbRawFile(_aare.CtbRawFile):
    """File reader for the CTB raw file format.
    
    Args:  
            fname (pathlib.Path | str): Path to the file to be read.
            chunk_size (int): Number of frames to read at a time. Default is 1.
            transform (function): Function to apply to the data after reading it. 
                The function should take a numpy array of type uint8 and return one
                or several numpy arrays.
    """
    def __init__(self, fname, chunk_size = 1, transform = None):
        super().__init__(fname)
        self._chunk_size = chunk_size
        self._transform = transform


    def read_frame(self, frame_index: int | None = None ) -> tuple:
        """Read one frame from the file and then advance the file pointer.

        .. note::

            Uses the position of the file pointer :py:meth:`~CtbRawFile.tell` to determine
            which frame to read unless frame_index is specified.

        Args:
            frame_index (int): If not None, seek to this frame before reading.

        Returns:
            tuple: header, data

        Raises:
            RuntimeError: If the file is at the end.
        """
        if frame_index is not None:
            self.seek(frame_index)


        header, data = super().read_frame()
        if header.shape == (1,):
            header = header[0]


        if self._transform:
            res = self._transform(data)
            if isinstance(res, tuple):
                return header, *res
            else:
                return header, res
        else:
            return header, data
            
    def read_n(self, n_frames:int) -> tuple:
        """Read several frames from the file.

        .. note::

            Uses the position of the file pointer :py:meth:`~CtbRawFile.tell` to determine
            where to start reading from.

        If the number of frames requested is larger than the number of frames left in the file,
        the function will read the remaining frames. If no frames are left in the file
        a RuntimeError is raised.

        Args:
            n_frames (int): Number of frames to read.

        Returns:
            tuple: header, data

        Raises:
            RuntimeError: If EOF is reached.
        """
        # Calculate the number of frames to actually read
        n_frames = min(n_frames, self.frames_in_file - self.tell())
        if n_frames == 0:
            raise RuntimeError("No frames left in file.")


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

    def read(self) -> tuple:
        """Read the entire file.
        Seeks to the beginning of the file before reading.

        Returns:
            tuple: header, data
        """
        self.seek(0)
        return self.read_n(self.frames_in_file)

    def seek(self, frame_index:int) -> None:
        """Seek to a specific frame in the file.

        Args:
            frame_index (int): Frame position in file to seek to.
        """
        super().seek(frame_index)

    def tell(self) -> int:
        """Return the current frame position in the file.

        Returns:
            int: Frame position in file.
        """
        return super().tell()
    
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
        return super().master()

    @property
    def image_size_in_bytes(self) -> int:
        """Return the size of the image in bytes.

        Returns:
            int: Size of image in bytes.
        """
        return super().image_size_in_bytes

    def __len__(self) -> int:
        """Return the number of frames in the file.

        Returns:
            int: Number of frames in file.
        """
        return super().frames_in_file
    
    @property
    def frames_in_file(self) -> int:
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

