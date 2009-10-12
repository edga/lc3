#!/usr/bin/env python
# Execute object file on FPGA with I/O redirection
#
# Based on tcp_redirect example from pySerial
#    (C) 2002-2006 Chris Liechti <cliechti@gmx.net>


import sys, os, serial, threading, time

try:
    True
except NameError:
    True = 1
    False = 0

def log(msg):
    sys.stderr.write('%s\n' % msg)

class Redirector:
    def __init__(self, serial, inRedirect, outRedirect):
        self.serial = serial
        self.inRedirect = inRedirect
        self.outRedirect = outRedirect
        self.writer_done = 0
        self.reader_done = 0

    def shortcut(self):
        """connect the serial port to the stdin/stdout by copying everything
           from one side to the other"""
        self.alive = True
        self.thread_read = threading.Thread(target=self.reader)
        #self.thread_read.setDaemon(1)
        self.thread_read.start()
        self.writer()
    
    def reader(self):
        """loop forever and copy serial->outRedirect"""
        while self.alive:
            try:
                data = self.serial.read(1)              #read one, blocking
                n = self.serial.inWaiting()             #look if there is more
                if n:
                    data = data + self.serial.read(n)   #and get as much as possible
                if data:
                    self.outRedirect.write(data)
            except Exception, msg:
                log(msg)
                break
        self.outRedirect.close()
        self.reader_done = 1
        self.alive = False
    
    def writer(self):
        """loop forever and copy inRedirect->serial"""
        while self.alive:
            try:
                data = self.inRedirect.readline()
                if not data:
                    break
                self.serial.write(data)                 #get a bunch of bytes and send them
            except Exception, msg:
                log(msg)
                break
        self.writer_done = 1
        log("Writer done. Waiting for 3 sec...")
        time.sleep(3)
        self.alive = False
        self.thread_read.join()

    def stop(self):
        """Stop copying"""
        if self.alive:
            self.alive = False
            self.thread_read.join()


if __name__ == '__main__':
    import optparse

    parser = optparse.OptionParser(usage="""\
Redirect serial communication to/from files.

%prog [options] inFile outFile""")

    parser.add_option("-O", "--objfile", dest="objFile", action="store",
        help="objfile, Upload objfile before starting redirection",
        default=None)

    parser.add_option("-s", "--stdio", dest="use_stdio", action="store_true",
        help="use_stdio, Use stdin/stdout instead of inFile/outFile",
        default=False)
    
    parser.add_option("-p", "--port", dest="port",
        help="port, a number (default COM1) or a device name (deprecated option)",
        default="COM1")
    
    parser.add_option("-b", "--baud", dest="baudrate", action="store", type='int',
        help="set baudrate, default 115200", default=115200)
        
    parser.add_option("", "--parity", dest="parity", action="store",
        help="set parity, one of [N, E, O], default=N", default='N')
    
    parser.add_option("", "--rtscts", dest="rtscts", action="store_true",
        help="enable RTS/CTS flow control (default off)", default=False)
    
    parser.add_option("", "--xonxoff", dest="xonxoff", action="store_true",
        help="enable software flow control (default off)", default=False)
    
    parser.add_option("", "--cr", dest="cr", action="store_true",
        help="do not send CR+LF, send CR only", default=False)
        
    parser.add_option("", "--lf", dest="lf", action="store_true",
        help="do not send CR+LF, send LF only (default)", default=True)
    
    parser.add_option("", "--rts", dest="rts_state", action="store", type='int',
        help="set initial RTS line state (possible values: 0, 1)", default=None)

    parser.add_option("", "--dtr", dest="dtr_state", action="store", type='int',
        help="set initial DTR line state (possible values: 0, 1)", default=None)

    parser.add_option("-q", "--quiet", dest="quiet", action="store_true",
        help="suppress non error messages", default=False)

    (options, args) = parser.parse_args()

    if options.cr and options.lf:
        parser.error("ony one of --cr or --lf can be specified")

    if not options.use_stdio:
        if len(args) < 2:
            parser.error("inFile and outFile args expected")
            sys.exit(1)
        inFile = args.pop(0)
        outFile = args.pop(0)

    if len(args) > 0:
        parser.error("too many arguments")
        sys.exit(1)

    ser = serial.Serial(options.port, options.baudrate, parity=options.parity, rtscts=options.rtscts, xonxoff=options.xonxoff, timeout=1)     #required so that the reader thread can exit 
    if not options.quiet:
        log("--- %s %s,%s,%s,%s ---" % (ser.portstr, ser.baudrate, 8, ser.parity, 1))

    try:
        ser.open()
    except serial.SerialException, e:
        log("Could not open serial port %s: %s" % (ser.portstr, e))
        sys.exit(1)

    if options.rts_state is not None:
        ser.setRTS(options.rts_state)

    if options.dtr_state is not None:
        ser.setDTR(options.dtr_state)

    if options.objFile:
        log("programming...")
        obj = open(options.objFile, "rb")
        ser.write(obj.read())
        log("programming done")

    if options.use_stdio:
        r = Redirector(ser, sys.stdin, sys.stdout)
    else:
        r = Redirector(ser, open(inFile,"rb"), open(outFile, "wb"))
        
    r.shortcut()
