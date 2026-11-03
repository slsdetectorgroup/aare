from pathlib import Path

from aare import JungfrauDataFile

import timeit 

import numpy as np

def read_backplane_pulsing_signals(backplane_pulsing_data : Path, num_signals : int = 220, num_acquisitions_per_signal : int = 10) -> np.ndarray:
    """
    Read the backplane pulsing signals from the raw data file and average over the number of acquisitions.

    Params: 
        backplane_pulsing_data : Path
            The path to the raw data file containing the backplane pulsing signals.
        num_signals : int, optional
            The number of backplane pulsing signals to read. Default is 220.
        num_acquisitions_per_signal : int, optional
            The number of acquisitions per signal. Default is 10.
    """  

    backplane_pulsing_file = JungfrauDataFile(backplane_pulsing_data)  #MultiThreadedFileReader(backplane_pulsing_data, ) #

    nrows, ncols = backplane_pulsing_file.rows, backplane_pulsing_file.cols

    backplane_pulsing_signals = np.empty(shape=(nrows, ncols, num_signals), dtype=np.uint16) # TODO should we use float? - copy in fit - maybe better for mean - not aligned with detector 
    # parallelizing over signals is harder - not sure if file_reading gets faster - cant use parallel file reading class either
    for signal_idx in range(num_signals): 
        _ , ADU_values = backplane_pulsing_file.read_n(num_acquisitions_per_signal) # TODO: use parallel file reader 
        # TODO doesnt fit into L3 cache - and update mean on the go in c++ 

        # maybe a bit unorthodox but could use seperate PedestalTrackingHistogram and process_pedestal_file for each signal and parallel file reading - also copy not neccessary 
        backplane_pulsing_signals[:, :, signal_idx] = np.mean(ADU_values, axis=0) # mean over acquisitions -> bad striding 

    backplane_pulsing_signals = backplane_pulsing_signals.transpose(1, 2, 0).copy() # transpose to (rows, cols, signals) and copy to avoid striding issues

    return backplane_pulsing_signals

num_calls = 10

my_file = Path("/home/mazzol_a/Documents/temp_jungfrau/"
        "BP_M749_2026-05-19_HGOG1G2_000000.dat")

time = timeit.timeit(
    lambda: read_backplane_pulsing_signals(my_file, num_signals=220, num_acquisitions_per_signal=10),
    number=num_calls
)


print(f"Per call: {time / num_calls:.6f} s")