from typing import Any


class Frame:
    """
    Frame class. uses proxy pattern to wrap around the pybinding class
    the intention behind it is to only use one class for frames in python (not Frame_8, Frame_16, etc) 
    """
    def __init__(self, frameImpl):
        self._frameImpl = frameImpl
    def __getattribute__(self, __name: str) -> Any:
        """
        Proxy pattern to call the methods of the frameImpl
        """
        return getattr(object.__getattribute__(self, "_frameImpl"), __name)
    