from typing import Any
import _aare

class DataSpan:
    def __init__(self, IFrame) -> None:
        bitdepth = IFrame.bitdepth
        class_name = f"_DataSpan_{bitdepth}"
        self.impl = getattr(_aare, class_name)(object.__getattribute__(IFrame, "impl"))
    
    def __getattr__(self, name: str) -> Any:
        return getattr(object.__getattribute__(self, "impl"), name)