# Set your customisations here:
#TEST_FPGA=1 # Run LC3 executable on the FPGA. Comment if simulation based test is prefered
PYTHON=/cygdrive/c/Python26/python
SERIAL_BAUD=115200
#SERIAL_BAUD=19200


# Use common file
BASEDIR=../
include $(BASEDIR)Make.common

LCC=lcc -g #-target=lc3dtu

SRC=$(wildcard *.c)
ASM=$(SRC:.c=.asm)
ASM_LCC=$(SRC:.c=.lcc)
OBJ=$(SRC:.c=.obj)

NO_INPUT_SRC=8q array fig14.1 fig15.1 fig15.2 fig16.1 fig16.4 incr sort spill struct switch
NO_INPUT_TESTS=$(NO_INPUT_SRC:=.test)
# $(patsubst %.c,%.o $(patsubst %.c,%.bar.c)


compile_all: $(ASM) $(ASM_LCC)


tests: $(NO_INPUT_TESTS)



clean:
	$(RM) *.exe *.obj *.sym *.asm *.lcc *.ser *.bin log.*.host log.*.lc3
	$(RM) $(NO_INPUT_TESTS)


