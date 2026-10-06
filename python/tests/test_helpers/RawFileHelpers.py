# SPDX-License-Identifier: MPL-2.0

import json
import shutil
import struct
import tempfile
import time
from pathlib import Path
from aare import ROI
import random


# TODO: maybe a fixture is better 
def get_module_size_from_roi(module_idx : tuple, receiver_roi : ROI, pixels_per_module: tuple) -> tuple:
    """Get the size of a module in pixels from the receiver ROI and the pixels per module.

    Args:
        module_idx (tuple): The index of the module in the format (y, x).
        receiver_roi (ROI): The receiver ROI.
        pixels_per_module (tuple): The number of pixels per module in the x and y directions.

    Returns:
        tuple: The size of the module in pixels in the x and y directions.
    """
    module_x = module_idx[1]
    module_y = module_idx[0]

    module_ROI = ROI(
        module_x * pixels_per_module[1], (module_x + 1) * pixels_per_module[1],
        module_y * pixels_per_module[0], (module_y + 1) * pixels_per_module[0]
    )

    # Calculate the size of the module in pixels
    xmin = max(receiver_roi.xmin, module_ROI.xmin)
    ymin = max(receiver_roi.ymin, module_ROI.ymin)
    xmax = min(receiver_roi.xmax, module_ROI.xmax)
    ymax = min(receiver_roi.ymax, module_ROI.ymax)


    if xmin >= xmax or ymin >= ymax:
        return (0, 0)
    else: 
        module_size_x = xmax - xmin
        module_size_y = ymax - ymin
        return (module_size_y, module_size_x)


class TemporaryJungfrauRawFiles:
    def __init__(self, modules: tuple = (2,1), pixels_per_module: tuple = (256,1024), receiver_roi : ROI = ROI(0, 1024, 0, 512)) -> None:
        unique = time.monotonic_ns()
        self._directory = Path(tempfile.gettempdir()) / f"aare-raw-{unique}"
        self._directory.mkdir()

        image_size_in_bytes = receiver_roi.size() * 2 # 2 bytes per pixel

        metadata = {
            "Version": 8.1,
            "Detector Type": "Jungfrau",
            "Timing Mode": "auto",
            "Geometry": {"x": modules[1], "y": modules[0]},
            "Image Size": image_size_in_bytes,
            "Pixels": {"x": pixels_per_module[1], "y": pixels_per_module[0]},
            "Max Frames Per File": 1,
            "Total Frames": 2,
            "Frames in File": 2,
            "Frame Padding": 1,
            "Frame Discard Policy": "nodiscard",
            "UDP Ports Type" : ["bottom", "top"],
            "UDP Ports Disabled": [], 
            "Receiver Rois": [{"xmin": receiver_roi.xmin, "xmax": receiver_roi.xmax-1, "ymin": receiver_roi.ymin, "ymax": receiver_roi.ymax-1}] # inclusive
        }
        self.master_path().write_text(json.dumps(metadata), encoding="utf-8")

        for module_x in range(metadata["Geometry"]["x"]):
            for module_y in range(metadata["Geometry"]["y"]):
                size = get_module_size_from_roi((module_y, module_x), receiver_roi, pixels_per_module)
                if(size == (0, 0)):
                    continue
                values = [random.randint(0, 65535) for _ in range(size[0] * size[1])]
                pixels = struct.pack(f"<{len(values)}H", *values)
                for file_index in range(metadata["Frames in File"]):
                    with self.data_path(module_x * metadata["Geometry"]["y"] + module_y, file_index).open("wb") as output:
                        output.write(self._detector_header_bytes(file_index)) 
                        output.write(pixels)

    def cleanup(self) -> None:
        shutil.rmtree(self._directory, ignore_errors=True)

    def __enter__(self) -> "TemporaryJungfrauRawFiles":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.cleanup()

    def __del__(self) -> None:
        self.cleanup()

    def master_path(self) -> Path:
        return self._directory / "run_master_0.json"

    def data_path(self, module: int = 0, file: int = 0) -> Path:
        return self._directory / f"run_d{module}_f{file}_0.raw"

    @staticmethod
    def _detector_header_bytes(frame_number: int) -> bytes:
        # TODO: Mirror include/aare/DetectorHeader.hpp exactly if the full
        # binary layout is required by the test.
        return struct.pack("<Q", frame_number) + bytes(range(104)) #in total detector header is 112 bytes, 8 bytes for frame number and 104 bytes for the rest of the header