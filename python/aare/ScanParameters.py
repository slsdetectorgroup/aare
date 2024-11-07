from . import _aare

class ScanParameters(_aare.ScanParameters):
    def __init__(self, s):
        super().__init__(s)
        
    def __iter__(self):
        return [getattr(self, a) for a in ['start', 'stop', 'step']].__iter__()
    
    def __str__(self):
        return f'ScanParameters({self.dac}: {self.start}, {self.stop}, {self.step})'
    
    def __repr__(self):
        return self.__str__()
    
    