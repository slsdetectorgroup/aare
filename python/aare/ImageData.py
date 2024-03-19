from typing import Any
import _aare

class ImageData:
    def __init__(self, IFrame):
        bitdepth = IFrame.bitdepth
        class_name = f"_ImageData_{bitdepth}"
        self.impl = getattr(_aare, class_name)(object.__getattribute__(IFrame, "impl"))
        
    def __getattr__(self, name: str) -> Any:
        return getattr(object.__getattribute__(self, "impl"), name)
        

        
