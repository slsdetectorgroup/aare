from pathlib import Path
from aare import JungfrauDataFile
import timeit
import numpy as np

def read_current_source_data(current_source_data : Path, num_samples, num_acq_per_exposure : int = 10) -> np.ndarray: 
    """
    Read the current source data from the raw data file and average over the number of acquisitions per exposure time.

    Parameters
    ----------
        current_source_data : Path
            The path to the raw data file containing the current source data.
        exposure_times : np.ndarray
            The exposure times to read.
        num_acq_per_exposure : int, optional
            The number of acquisitions per exposure time. Default is 10.
        output_file : Path, optional
            The path to the file where the averaged current source data will be saved. If None, the data is not saved. Default is None.
    Returns
    -------
        np.ndarray
            A 3D array containing the averaged current source data for each exposure time. The shape of the array is (rows, cols, num_exposure_times)
    """

    num_exposure_times = num_samples
    columns_per_subcolumn : int = 64 # number of columns per subcolumn - 64 for Jungfrau 1M - Jungfrau has 4 subcolumns per module a 64 columns 

    cs_file = JungfrauDataFile(current_source_data)

    if ((num_exposure_times + 3) * num_acq_per_exposure * columns_per_subcolumn != cs_file.total_frames):
        raise ValueError(f"The number of frames in the current source data file does not match the expected number of frames. Expected: {(num_exposure_times + 3) * num_acq_per_exposure * columns_per_subcolumn}, got: {cs_file.total_frames}")

    mean_cs = np.zeros((cs_file.rows, cs_file.cols, num_exposure_times), dtype=np.float64) # TODO: use mean and np.uint16

    cs_file.seek(3*num_acq_per_exposure*columns_per_subcolumn) # skip first 3 frames used to check gain switching 
    
    for i in range(num_exposure_times):
        for _ in range(num_acq_per_exposure):
            for k in range(columns_per_subcolumn):
                _, frame = cs_file.read_frame()
                mean_cs[:256, k:-1:columns_per_subcolumn, i] += frame[:256, k:-1:columns_per_subcolumn]
                # flipped for top modules
                flipped_k = columns_per_subcolumn - 1 - k
                mean_cs[256:, flipped_k:-1:columns_per_subcolumn, i] += frame[256:, flipped_k:-1:columns_per_subcolumn]

        mean_cs[:, :, i] /= num_acq_per_exposure

    return mean_cs.astype(np.uint16).copy() 

num_calls = 10

my_file = Path("/home/mazzol_a/Documents/temp_jungfrau/"
        "CS_M749_2026-05-19_HGOG1G2_000000.dat")

time = timeit.timeit(
    lambda: read_current_source_data(my_file, 97, num_acq_per_exposure=10),
    number=num_calls
)


print(f"Per call: {time / num_calls:.6f} s")