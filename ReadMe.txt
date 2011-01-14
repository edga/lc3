This is LC3 toolchain sources adapted for DTU
=============================================

Content
-------

  lc3db: gdb emulation of LC3 simulator (used with DDD front end)
  lc3tools: LC3 assembler+simulator from book accompanying web-site
  lcc-1.3: LCC based C compiler for LC3



Chages in lc3tools (assembler+simulator)
----------------------------------------

  0. Build script (configure/Makefile.def) modified to enable compilation to pure windows executables (using "--no-cygwin" option of the cygwin toolchain)

  1. Bug fixes:
     1.1. Object files were opened in "text" mode, causing corruption of generated files on windows (each '0x0D' byte was written as sequence of '0x0D' '0x0A').
     1.2. Where were no proper checks for strtol() returning error.
     1.3. Range checking for immediate values was not done correctly:
     		"ADD R1, R0, 31" was accepted as valid (though 15 is the highest allowed constant). The code generated would be "ADD R1, R0, -1".

  2. Extra instructions added: 
     [See: "Extending Assembler" later]
     2.1. Syntactic sugar:
		NOP:       alias to ".FILL x0000"
		.BLKWTO:   version of .BLKW which reserves memory until offset instead of reserving count words (used to align code to given address)
     2.2. Immediate shifts:
        	"SLL/SRA <imm4u>" are implemented in unused bits of ADD/AND:	
      			SLL: 0001.dr.sr1.01.imm4u
      			SRA: 0101.dr.sr1.01.imm4u
     2.3. Extra RRR format (3 reg) arithmetics:
     		SLL,SRA,DIV,MOD and MUL implemented in reserved instruction:
			1101.dr.sr1.func.sr2
		where 3bit 'func' field is 0,1,2... for SLL,SRA,DIV,MOD and MUL

  3. Extra files generated:
     3.1. "*.bin" file used for inclusion from VHDL code
     	  Each word is written as text line of 16 characters ('0' or '1')
     3.2. "*.ser" file used to upload file to the FPGA
          The first word is size of the data/code words. The rest is the same as in "*.obj" file: Offset, {Code/Data}*

  4. Input enable simulator:
     The simulator was using standard input for both control commands and to read the inputs for the program. It was also flushing input on command read, so it was hard to run a test with input.
     Extra command line options was added to use the input for the program from the separate file.


 
Chages in lcc-1.3 (C compiler)
------------------------------

  0. Enable compilation to pure windows executables:
     0.1 Build script (configure/Makefile.def) modified to use "--no-cygwin" option of the cygwin toolchain
     0.2 Paths in the source code were changed to use either unix or windows directory separators.

  1. Bug Fixes:
     See: "LC3 Evaluation Report" (separate file).

  2. Target with DTU processor extensions added to allow generation of more compact and efficient code:
     lcc-1.3/src/lc3.md was forked to lc3dtu.md. See: "Extending Compiler"
     To invoke generation of code with extensions add "-target=lc3dtu" option to the lcc commandline. 

  3. Removed linking of unused backends (sparc/mips/x86...), they wouldn't work anyway, because lc3 backend porters modified front end (mainly changed the limits for the types).


Extending Assembler
-------------------

Extending of assembler is fairly simple, the core code can be found in "lc3tools/lc3.f" which is flex processed C source. There is also a code for managing symbols (symbol.c), but it's modification was not needed.

To add/modify instructions one needs:
	1. Add new commands: Constants in "opcode_t" and names in "opnames" array. Make sure that they occur in the same order.
	2. Add format of accepted arguments for your command in "op_format_ok" array. Again make sure to use "opcode_t" order.
	3. Add appropriate flex rules to trigger your code then command keyword is read in the assembly source. It sounds complicated, but is simple, no knowledge of lex is needed, just look at neighboring lines :)
	4. Add case statements to generate extra instructions in the "generate_instruction()" function. Again this is fairly simple, just look at the neighboring code.
	 
To facilitate further modification, modifiable fragments (corresponding to steps just mentioned) are marked (in lc3.f) with following comment blocks:

	 /***************************/
	 /*	DTU extensions	    */

	   ... Change code  here ...

	 /*			    */
	 /***************************/



Extending Compiler
------------------

Extending the compiler is a little bit more involving. It is bigger peace of code, composed of more files. Using few stages to compile the program, and invoking other programs (assembler).
The compilation procedure for lc3 target can be seen when lcc is invoked with "-v" option and is following:

	*preprocessor* (cpp: lcc-1.3/cpp/cpp.c) merges all the includes and generates single C file.

	*code generator* (rcc: lcc-1.3/main.c) with appropriate target (lc3 or lc3dtu) transforms single C file to abstract LC3 assembly file ("*.lcc").
	    Exact placement of variables is not known yet, so special directive is used to denote variable offset read. For example, ".LC3GLOBAL L2_sort 7" means, to load offset of the "L2_sort" symbol to the register 7. 

	*lc3 postprocessor* (lc3pp: lcc-1.3/lc3pp/lc3pp.c) accepts "*.lcc" source, creates data segment, populates it with variables and fills appropriate code in place of ".LCGLOBAL" directives.
	    It also tries to link missing external functions, by looking them with "<function>.asm" name in the "lc3lib" folder. This way implementation of libc functions like "printf()" is connected.
	    The generated file is well formed and complete LC3 assembly source.

 	*lc3 assembler* (lc3as: lc3tools/lc3.f) generates "*.obj", "*.sym" and the rest.

LCC framework is well designed, so one should need only to modify backend (lc3.md or lc3dtu.md) and lc3 postprocessor (lc3pp.c).
The LCC is documented in the book, but some local modifications are possible without knowing all the details.

<TODO: add links to backend interface v.4 documentation and links to relevant chapters of the book>
<TODO: Say few words about instruction tree cover rules>
<TODO: Few words about some important functions like: prologue, emit2>

