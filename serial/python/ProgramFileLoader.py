import os
from array import array

class ProgramFileLoader:
    """Loads LC3 object file for serial programming"""

    class Error(Exception):
        def __init__(self, error, caption="Program file error"):
            self.caption = caption
            self.error = error 
        def __str__(self):
            return "%s: %s" % (self.caption, self.error)
        
    def __init__(self, path):
        """Format of data file is:
               <DataWordsCount><DestinationOffset><DataWord1><DataWord2>..."""
        self.path = path
        size = os.path.getsize(path)
        if size < 6 or size % 2:
            raise self.Error("Size of selected file has to be multiple of 2 and file must containt at least 3 words", "File size error")
        size = size/2

        fdata = open(path, "rb")
        data = array('H')
        data.fromfile(fdata, size)
        fdata.close()
        if "foreign_endian":
            data.byteswap()

        if data[0] != (size-2):
            raise self.Error("Count field (%d) doesn't match file size (%d)" % (data[0], size-2), "File size error")

        self.chunks = []
        self.chunks.append( (data[0], data[1], data[2:]) )
        
