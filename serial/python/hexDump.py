#!/usr/bin/python
import os
from array import array


def dumpLine(offset, line):
    print ("%04x:  " % (offset)) + \
           "%04x %04x %04x %04x  %04x %04x %04x %04x" % tuple(line)
        
def dumpFile(path):
        size = os.path.getsize(path)/2
        fdata = open(path, "rb")
        data = array('H')
        data.fromfile(fdata, size)
        fdata.close()
        if "foreign_endian":
            data.byteswap()
        # use first word as offset
        offset = data[0]

        if size % 8:
            for i in range(9-(size % 8)):
                data.append(0)
        size = (size+7)/8;

        ll = l = []
        loffset = 0
        for i in range(size):
            s = (i*8) + 1
            l = list(data[s:s+8])
            #print "%i: %s" % (i,l)
            if l != ll:
                if loffset < offset:
                    print "*"
                dumpLine(offset, l)
                ll = l
                loffset = offset+8
            offset += 8
        dumpLine(offset, l)
         


if __name__ == '__main__':
    import sys

    if len(sys.argv) != 2:
        print("Usage:\n"+
              "  %s LC3_OBJ_FILE\n")
    else:
        dumpFile(sys.argv[1]);
        
