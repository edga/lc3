/*									tab:8
 *
 * lc3.f - lexer for the LC-3 assembler
 *
 * "Copyright (c) 2003 by Steven S. Lumetta."
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written 
 * agreement is hereby granted, provided that the above copyright notice
 * and the following two paragraphs appear in all copies of this software,
 * that the files COPYING and NO_WARRANTY are included verbatim with
 * any distribution, and that the contents of the file README are included
 * verbatim as part of a file named README with any distribution.
 * 
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE TO ANY PARTY FOR DIRECT, 
 * INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT 
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE AUTHOR 
 * HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE AUTHOR SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" 
 * BASIS, AND THE AUTHOR NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, 
 * UPDATES, ENHANCEMENTS, OR MODIFICATIONS."
 *
 * Author:	    Steve Lumetta
 * Version:	    1
 * Creation Date:   18 October 2003
 * Filename:	    lc3.f
 * History:
 *	SSL	1	18 October 2003
 *		Copyright notices and Gnu Public License marker added.
 */

%option noyywrap nounput

%{

/* questions...

should the assembler allow colons after label names?  are the colons
part of the label?  Currently I allow only alpha followed by alphanum and _.

*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include "symbol.h"

typedef enum opcode_t opcode_t;
enum opcode_t {
    /* no opcode seen (yet) */
    OP_NONE,

    /* real instruction opcodes */
    OP_ADD, OP_AND, OP_BR, OP_JMP, OP_JSR, OP_JSRR, OP_LD, OP_LDI, OP_LDR,
    OP_LEA, OP_NOT, OP_RTI, OP_ST, OP_STI, OP_STR, OP_TRAP,

    /* trap pseudo-ops */
    OP_GETC, OP_HALT, OP_IN, OP_OUT, OP_PUTS, OP_PUTSP,

    /* non-trap pseudo-ops */
    OP_FILL, OP_RET, OP_STRINGZ,

    /* directives */
    OP_BLKW, OP_END, OP_ORIG, 

/************************************************************************/
/*		DTU extensions						*/
/* Syntactic sugar:							*/
/*   NOP:       alias to BR with nzp cleared (0x0000 is used here)	*/
/*   .BLKWTO:   version of .BLKW which reserves memory untill offset    */
/*              instead of reserving count words                       	*/
/*									*/
/* Immediate shifts:							*/
/*   "SLL/SRA <imm4u>" are implemented in unused bits of ADD/AND:	*/
/* 	SLL: 0001.dr.sr1.01.imm4u					*/
/* 	SRA: 0101.dr.sr1.01.imm4u					*/
/*									*/
/* Extra RRR format (3 reg) arithmetics:				*/
/*   SLL,SRA,DIV,MOD and MUL implemented in reserved instruction:	*/
/*	     1101.dr.sr1.func.sr2					*/
/*	where 3bit func field is 0,1,2... for SLL,SRA,DIV,MOD and MUL	*/
/*									*/

    OP_SLL,	/* Shift Left Logical					*/
    OP_SRA,	/* Shift Right Arithmetic				*/
    OP_DIV,	/* Divide						*/
    OP_MOD,	/* Remainder						*/
    OP_MUL,	/* Multiply						*/

    /* pseudo-ops */
    OP_NOP,	/* No operation						*/
    
    /* directives */
    OP_BLKWTO,
/*									*/
/************************************************************************/

    NUM_OPS
};

static const char* const opnames[NUM_OPS] = {
    /* no opcode seen (yet) */
    "missing opcode",

    /* real instruction opcodes */
    "ADD", "AND", "BR", "JMP", "JSR", "JSRR", "LD", "LDI", "LDR", "LEA",
    "NOT", "RTI", "ST", "STI", "STR", "TRAP",

    /* trap pseudo-ops */
    "GETC", "HALT", "IN", "OUT", "PUTS", "PUTSP",

    /* non-trap pseudo-ops */
    ".FILL", "RET", ".STRINGZ",

    /* directives */
    ".BLKW", ".END", ".ORIG",

/************************************************/
/*		DTU extensions			*/
    "SLL",	/* Shift Left Logical		*/
    "SRA",	/* Shift Right Arithmetic	*/
    "DIV",	/* Divide			*/
    "MOD",	/* Remainder			*/
    "MUL",	/* Multiply			*/

    /* pseudo-ops */
    "NOP",	/* No operation			*/
 
    /* directives */
    ".BLKWTO",  /* Fill value untill address    */
/*						*/
/************************************************/
};

typedef enum ccode_t ccode_t;
enum ccode_t {
    CC_    = 0,
    CC_P   = 0x0200,
    CC_Z   = 0x0400,
    CC_N   = 0x0800
};

typedef enum operands_t operands_t;
enum operands_t {
    O_RRR, O_RRI,
    O_RR,  O_RI,  O_RL,
    O_R,   O_I,   O_L,   O_S,
    O_,
    NUM_OPERANDS
};

static const int op_format_ok[NUM_OPS] = {
    /* no opcode seen (yet) */
    0x200, /* no opcode, no operands       */

    /* real instruction formats */
    0x003, /* ADD: RRR or RRI formats only */
    0x003, /* AND: RRR or RRI formats only */
    0x0C0, /* BR: I or L formats only      */
    0x020, /* JMP: R format only           */
    0x0C0, /* JSR: I or L formats only     */
    0x020, /* JSRR: R format only          */
    0x018, /* LD: RI or RL formats only    */
    0x018, /* LDI: RI or RL formats only   */
    0x002, /* LDR: RRI format only         */
    0x018, /* LEA: RI or RL formats only   */
    0x004, /* NOT: RR format only          */
    0x200, /* RTI: no operands allowed     */
    0x018, /* ST: RI or RL formats only    */
    0x018, /* STI: RI or RL formats only   */
    0x002, /* STR: RRI format only         */
    0x040, /* TRAP: I format only          */

    /* trap pseudo-op formats (no operands) */
    0x200, /* GETC: no operands allowed    */
    0x200, /* HALT: no operands allowed    */
    0x200, /* IN: no operands allowed      */
    0x200, /* OUT: no operands allowed     */
    0x200, /* PUTS: no operands allowed    */
    0x200, /* PUTSP: no operands allowed   */

    /* non-trap pseudo-op formats */
    0x0C0, /* .FILL: I or L formats only   */
    0x200, /* RET: no operands allowed     */
    0x100, /* .STRINGZ: S format only      */

    /* directive formats */
    0x040, /* .BLKW: I format only         */
    0x200, /* .END: no operands allowed    */
    0x040  /* .ORIG: I format only         */

/************************************************/
/*		DTU extensions			*/
    ,
    0x003, /* SLL: RRR format (in RESERV opcode)*/
	   /*      RRI format (in ADD opcode)   */
    0x003, /* SRA: RRR format (in RESERV opcode)*/
	   /*      RRI format (in AND opcode)   */
    0x001, /* DIV: RRR format only		*/
    0x001, /* MOD: RRR format only		*/
    0x001, /* MUL: RRR format only		*/

    /* pseudo-ops */
    0x200, /* NOP: no operands allowed		*/
 
    /* directives */
    0x040  /* .BLKWTO: I format only            */
/*						*/
/************************************************/
};

typedef enum pre_parse_t pre_parse_t;
enum pre_parse_t {
    NO_PP =  0,
    PP_R1 =  1,
    PP_R2 =  2,
    PP_R3 =  4,
    PP_I2 =  8,
    PP_L2 = 16
};

typedef enum debug_stab_t debug_stab_t;
enum debug_stab_t {
    D_FILE,
    D_LINE,
    D_LINE_END,
    D_TYPE,
    D_SYMBOL,
    D_BLOCK
};


static const pre_parse_t pre_parse[NUM_OPERANDS] = {
    (PP_R1 | PP_R2 | PP_R3), /* O_RRR */
    (PP_R1 | PP_R2),         /* O_RRI */
    (PP_R1 | PP_R2),         /* O_RR  */
    (PP_R1 | PP_I2),         /* O_RI  */
    (PP_R1 | PP_L2),         /* O_RL  */
    PP_R1,                   /* O_R   */
    NO_PP,                   /* O_I   */
    NO_PP,                   /* O_L   */
    NO_PP,                   /* O_S   */
    NO_PP                    /* O_    */
};

typedef struct inst_t inst_t;
struct inst_t {
    opcode_t op;
    ccode_t  ccode;
};

static int pass, line_num, num_errors, saw_orig, code_loc, saw_end;
static int code_orig;
static inst_t inst;
static char *source_name = NULL;
static char *last_label = NULL;
static char *last_cmd = NULL;
static char instr_disasm[512];
static FILE* symout;
static FILE* objout;
static FILE* dbgout;
static FILE* lstout;
/*
 * "base.bin" file used to initialize content of memory in VHDL code.
 * Each generated word is output on separate line as sequence of 16 characters ('1' or '0')
 */
static FILE* binout;
static FILE* vcout;
static int vcout_line_addr;

/* debug support */
enum dbg_line_state {
    DBG_LINE_NONE = 0,
    DBG_LINE_PARSED,
    DBG_LINE_STARTED,
    DBG_LINE_FINISHED
};
typedef enum dbg_line_state dbg_line_state_t;

static dbg_line_state_t dbg_line_state;
static char dbg_line_info[256];
static int dbg_line_start_addr;
static void handle_debug (debug_stab_t stype, char* opstr);

static void new_inst_line ();
static void bad_operands ();
static void unterminated_string ();
static void bad_line ();
static void line_ignored ();
static void parse_ccode (const char*);
static void generate_instruction (operands_t, const char*);
static void found_label (const char* lname);

static void show_error (const char *format, ...)
      __attribute__ ((format (printf, 1, 2)));

static void show_error(const char* format, ...)
{
    va_list args;
    
    va_start(args,format);
    fprintf (stderr, "%s:%3d: ", source_name, line_num);
    vfprintf (stderr, format, args );
    va_end(args);
}
    


%}

/* condition code specification */
CCODE    [Nn]?[Zz]?[Pp]?

/* operand types */
REGISTER [rR][0-7]
HEX      [xX][-]?[0-9a-fA-F]+
DECIMAL  [#]?[-]?[0-9]+
IMMED    {HEX}|{DECIMAL}
LABEL    [_A-Za-z][A-Za-z_0-9]*
STRING   \"([^\"]*|(\\\"))*\"
UTSTRING \"[^\n\r]*

/* operand and white space specification */
SPACE     [ \t]
OP_SEP    {SPACE}*[, \t]{SPACE}*
COMMENT   [;][^\n\r]*
EMPTYLINE {SPACE}*{COMMENT}?
ENDLINE   {EMPTYLINE}\r?\n\r?
RESTLINE  [^\n\r]*{ENDLINE}

/* operand formats */
O_RRR  {SPACE}+{REGISTER}{OP_SEP}{REGISTER}{OP_SEP}{REGISTER}{ENDLINE}
O_RRI  {SPACE}+{REGISTER}{OP_SEP}{REGISTER}{OP_SEP}{IMMED}{ENDLINE}
O_RR   {SPACE}+{REGISTER}{OP_SEP}{REGISTER}{ENDLINE}
O_RI   {SPACE}+{REGISTER}{OP_SEP}{IMMED}{ENDLINE}
O_RL   {SPACE}+{REGISTER}{OP_SEP}{LABEL}{ENDLINE}
O_R    {SPACE}+{REGISTER}{ENDLINE}
O_I    {SPACE}+{IMMED}{ENDLINE}
O_L    {SPACE}+{LABEL}{ENDLINE}
O_S    {SPACE}+{STRING}{ENDLINE}
O_UTS  {SPACE}+{UTSTRING}{ENDLINE}
O_     {ENDLINE}

/* need to define YY_INPUT... */

/* exclusive lexing states to read operands, eat garbage lines, and
   check for extra text after .END directive */
%x ls_operands ls_garbage ls_finished
/* lexing states for debug info */
%x ls_debug_file ls_debug_line ls_debug_lineend ls_debug_type ls_debug_symbol ls_debug_block

%%

    /* rules for real instruction opcodes */
ADD       {inst.op = OP_ADD;  last_cmd = "ADD"; BEGIN (ls_operands);}
AND       {inst.op = OP_AND;  last_cmd = "AND"; BEGIN (ls_operands);}
BR{CCODE} {inst.op = OP_BR;   parse_ccode (yytext + 2); BEGIN (ls_operands);}
JMP       {inst.op = OP_JMP;  last_cmd = "JMP"; BEGIN (ls_operands);}
JSRR      {inst.op = OP_JSRR; last_cmd = "JSRR"; BEGIN (ls_operands);}
JSR       {inst.op = OP_JSR;  last_cmd = "JSR"; BEGIN (ls_operands);}
LDI       {inst.op = OP_LDI;  last_cmd = "LDI"; BEGIN (ls_operands);}
LDR       {inst.op = OP_LDR;  last_cmd = "LDR"; BEGIN (ls_operands);}
LD        {inst.op = OP_LD;   last_cmd = "LD";  BEGIN (ls_operands);}
LEA       {inst.op = OP_LEA;  last_cmd = "LEA"; BEGIN (ls_operands);}
NOT       {inst.op = OP_NOT;  last_cmd = "NOT"; BEGIN (ls_operands);}
RTI       {inst.op = OP_RTI;  last_cmd = "RTI"; BEGIN (ls_operands);}
STI       {inst.op = OP_STI;  last_cmd = "STI"; BEGIN (ls_operands);}
STR       {inst.op = OP_STR;  last_cmd = "STR"; BEGIN (ls_operands);}
ST        {inst.op = OP_ST;   last_cmd = "ST";  BEGIN (ls_operands);}
TRAP      {inst.op = OP_TRAP; last_cmd = "TRAP"; BEGIN (ls_operands);}

    /* rules for trap pseudo-ols */
GETC      {inst.op = OP_GETC; last_cmd = "GETC"; BEGIN (ls_operands);}
HALT      {inst.op = OP_HALT; last_cmd = "HALT"; BEGIN (ls_operands);}
IN        {inst.op = OP_IN;   last_cmd = "IN"; BEGIN (ls_operands);}
OUT       {inst.op = OP_OUT;  last_cmd = "OUT"; BEGIN (ls_operands);}
PUTS      {inst.op = OP_PUTS; last_cmd = "PUTS"; BEGIN (ls_operands);}
PUTSP     {inst.op = OP_PUTSP;last_cmd = "PUTSP"; BEGIN (ls_operands);}

    /* rules for non-trap pseudo-ops */
\.FILL    {inst.op = OP_FILL; last_cmd = ".FILL"; BEGIN (ls_operands);}
RET       {inst.op = OP_RET;  last_cmd = "RET"; BEGIN (ls_operands);}
\.STRINGZ {inst.op = OP_STRINGZ; last_cmd = ".STRINGZ"; BEGIN (ls_operands);}

    /* rules for directives */
\.BLKW    {inst.op = OP_BLKW; last_cmd = ".BLKW"; BEGIN (ls_operands);}
\.END     {saw_end = 1;       last_cmd = NULL; BEGIN (ls_finished);}
\.ORIG    {inst.op = OP_ORIG; last_cmd = NULL; BEGIN (ls_operands);}

 /***********************************************/
 /*		DTU extensions			*/
SLL       {inst.op = OP_SLL;   last_cmd = "SLL"; BEGIN (ls_operands);}
SRA       {inst.op = OP_SRA;   last_cmd = "SRA"; BEGIN (ls_operands);}
DIV       {inst.op = OP_DIV;   last_cmd = "DIV"; BEGIN (ls_operands);}
MOD       {inst.op = OP_MOD;   last_cmd = "MOD"; BEGIN (ls_operands);}
MUL       {inst.op = OP_MUL;   last_cmd = "MUL"; BEGIN (ls_operands);}

    /* pseudo-ops */
NOP       {inst.op = OP_NOP;   last_cmd = "NOP"; BEGIN (ls_operands);}

    /* directives */
\.BLKWTO  {inst.op = OP_BLKWTO;last_cmd = ".BLKWTO";  BEGIN (ls_operands);}

 /*						*/
 /***********************************************/

    /* debug support */
\.DEBUG{SPACE}+FILE{SPACE}+ { BEGIN (ls_debug_file);}
\.DEBUG{SPACE}+LINE{SPACE}+ { BEGIN (ls_debug_line);}
\.DEBUG{SPACE}+LINEEND{SPACE}+ { BEGIN (ls_debug_lineend);}
\.DEBUG{SPACE}+TYPE{SPACE}+ { BEGIN (ls_debug_type);}
\.DEBUG{SPACE}+SYMBOL{SPACE}+ { BEGIN (ls_debug_symbol);}
\.DEBUG{SPACE}+BLOCK{SPACE}+ { BEGIN (ls_debug_block);}

<ls_debug_file>{DECIMAL}:{RESTLINE}             { handle_debug (D_FILE, yytext); new_inst_line(); BEGIN (0);}
<ls_debug_line>{DECIMAL}:{DECIMAL}{ENDLINE}     { handle_debug (D_LINE, yytext); new_inst_line(); BEGIN (0);}
<ls_debug_lineend>{DECIMAL}:{DECIMAL}{ENDLINE}  { handle_debug (D_LINE_END, yytext); new_inst_line(); BEGIN (0);}
<ls_debug_type>{DECIMAL}={RESTLINE}             { handle_debug (D_TYPE, yytext); new_inst_line(); BEGIN (0);}
<ls_debug_symbol>[FfGSslp]{DECIMAL}:[^:]+:{RESTLINE}  { handle_debug (D_SYMBOL, yytext); new_inst_line(); BEGIN (0);}
<ls_debug_block>{RESTLINE}  { handle_debug (D_BLOCK, yytext); new_inst_line(); BEGIN (0);}

    /* rules for operand formats */
<ls_operands>{O_RRR} {generate_instruction (O_RRR, yytext); BEGIN (0);}
<ls_operands>{O_RRI} {generate_instruction (O_RRI, yytext); BEGIN (0);}
<ls_operands>{O_RR}  {generate_instruction (O_RR, yytext);  BEGIN (0);}
<ls_operands>{O_RI}  {generate_instruction (O_RI, yytext);  BEGIN (0);}
<ls_operands>{O_RL}  {generate_instruction (O_RL, yytext);  BEGIN (0);}
<ls_operands>{O_R}   {generate_instruction (O_R, yytext);   BEGIN (0);}
<ls_operands>{O_I}   {generate_instruction (O_I, yytext);   BEGIN (0);}
<ls_operands>{O_L}   {generate_instruction (O_L, yytext);   BEGIN (0);}
<ls_operands>{O_S}   {generate_instruction (O_S, yytext);   BEGIN (0);}
<ls_operands>{O_}    {generate_instruction (O_, yytext);    BEGIN (0);}

    /* eat excess white space */
{SPACE}+ {}  
{ENDLINE} {new_inst_line (); /* a blank line */ }

    /* labels, with or without subsequent colons */\
    /* 
       the colon form is used in some examples in the second edition
       of the book, but may be removed in the third; it also allows 
       labels to use opcode and pseudo-op names, etc., however.
     */
{LABEL}          {found_label (yytext);}
{LABEL}{SPACE}*: {found_label (yytext);}

    /* error handling??? */
<ls_operands>{O_UTS} {unterminated_string (); BEGIN (0);}
<ls_operands>[^\n\r]*{ENDLINE} {bad_operands (); BEGIN (0);}
{O_RRR}|{O_RRI}|{O_RR}|{O_RI}|{O_RL}|{O_R}|{O_I}|{O_S}|{O_UTS} {
    bad_operands ();
}

. {BEGIN (ls_garbage);}
<ls_garbage>[^\n\r]*{ENDLINE} {bad_line (); BEGIN (0);}

    /* parsing after the .END directive */
<ls_finished>{ENDLINE}|{EMPTYLINE}     {new_inst_line (); /* a blank line  */}
<ls_finished>.*({ENDLINE}|{EMPTYLINE}) {line_ignored (); return 0;}

%%

int
main (int argc, char** argv)
{
    int len;
    char* ext;
    char* fname;

    if (argc != 2) {
        fprintf (stderr, "usage: %s <ASM filename>\n", argv[0]);
	return 1;
    }

    /* Make our own copy of the filename. */
    len = strlen (argv[1]);
    if ((fname = malloc (len + 5)) == NULL) {
        perror ("malloc");
	return 3;
    }
    strcpy (fname, argv[1]);

    /* Check for .asm extension; if not found, add it. */
    if ((ext = strrchr (fname, '.')) == NULL || strcmp (ext, ".asm") != 0) {
	ext = fname + len;
        strcpy (ext, ".asm");
    }
    source_name = strdup(fname);

    /* Open input file. */
    if ((lc3in = fopen (fname, "r")) == NULL) {
        fprintf (stderr, "Could not open %s for reading.\n", fname);
	return 2;
    }

    /* Open output files. */
    strcpy (ext, ".obj");
    if ((objout = fopen (fname, "wb")) == NULL) {
        fprintf (stderr, "Could not open %s for writing.\n", fname);
	return 2;
    }
    strcpy (ext, ".bin");
    if ((binout = fopen (fname, "w")) == NULL) {
        fprintf (stderr, "Could not open %s for writing.\n", fname);
	return 2;
    }
    strcpy (ext, ".lst");
    if ((lstout = fopen (fname, "w")) == NULL) {
        fprintf (stderr, "Could not open %s for writing.\n", fname);
	return 2;
    }
    strcpy (ext, ".vconst");
    if ((vcout = fopen (fname, "w")) == NULL) {
        fprintf (stderr, "Could not open %s for writing.\n", fname);
	return 2;
    }
    strcpy(ext, ".dbg");
    if ((dbgout = fopen (fname, "w")) == NULL) {
	 fprintf (stderr, "Could not open %s for writing.\n", fname);
	 return 2;
    } else {
	char buf[512];
        char *cwd = getcwd(buf, sizeof(buf));
        fprintf(dbgout, "#0:%s/%s\n", cwd, argv[1]);
    }
    strcpy (ext, ".sym");
    if ((symout = fopen (fname, "w")) == NULL) {
        fprintf (stderr, "Could not open %s for writing.\n", fname);
	return 2;
    } else {
        /* FIXME: Do we really need to exactly match old format for compatibility 
           with Windows simulator? */
        fprintf (symout, "// Symbol table\n");
        fprintf (symout, "// Scope level 0:\n");
        fprintf (symout, "//\tSymbol Name       Page Address\n");
        fprintf (symout, "//\t----------------  ------------\n");
    }

    puts ("STARTING PASS 1");
    pass = 1;
    line_num = 0;
    num_errors = 0;
    saw_orig = 0;
    code_loc = 0x3000;
    saw_end = 0;
    new_inst_line ();
    yylex ();
    if (saw_orig == 0) {
        if (num_errors == 0 && !saw_end)
	    show_error("file contains only comments\n");
        else {
	    if (saw_end == 0)
		show_error("no .ORIG or .END directive found\n");
	    else
		show_error("no .ORIG directive found\n");
	}
	num_errors++;
    } else if (saw_end == 0 ) {
	show_error("no .END directive found\n");
	num_errors++;
    }
    printf ("%d errors found in first pass.\n", num_errors);
    if (num_errors > 0)
    	return 1;
    if (fseek (lc3in, 0, SEEK_SET) != 0) {
        perror ("fseek to start of ASM file");
	return 3;
    }

    yyrestart (lc3in);
    /* Return lexer to initial state.  It is otherwise left in ls_finished
       if an .END directive was seen. */
    BEGIN (0);

    puts ("STARTING PASS 2");
    pass = 2;
    dbg_line_state = DBG_LINE_NONE;
    line_num = 0;
    num_errors = 0;
    saw_orig = 0;
    code_loc = 0x3000;
    saw_end = 0;
    new_inst_line ();
    yylex ();
    printf ("%d errors found in second pass.\n", num_errors);
    if (num_errors > 0)
    	return 1;

    fprintf (symout, "\n");
    fclose (symout);
    fclose (objout);
    fclose (binout);
    /* dbgout: finish pending line */
    if (dbg_line_state==DBG_LINE_STARTED) {
        fprintf (dbgout, "@%s:%.4x:%.4x\n", dbg_line_info, dbg_line_start_addr, code_loc-1);
    }
    fclose (dbgout);
    /* VHDL constants file */
    if (vcout_line_addr <= code_loc) {
        fprintf(vcout, " -- addr 0x%04x to 0x%04x\nothers => X\"0000\"\n", vcout_line_addr, code_loc);
    } else
    	fprintf(vcout, "others => X\"0000\"\n");
    fclose(vcout);

    return 0;
}

static void
new_inst_line () 
{
    free(last_label);
    last_label = NULL;
    instr_disasm[0] = 0;
    inst.op = OP_NONE;
    inst.ccode = CC_;
    line_num++;
}

static void
bad_operands ()
{
    show_error("illegal operands for %s\n", opnames[inst.op]);
    num_errors++;
    new_inst_line ();
}

static void
unterminated_string ()
{
    show_error("unterminated string\n");
    num_errors++;
    new_inst_line ();
}

static void 
bad_line ()
{
    show_error("contains unrecognizable characters\n");
    num_errors++;
    new_inst_line ();
}

static void 
line_ignored ()
{
    if (pass == 1)
	show_error("WARNING: all text after .END ignored\n");
}

static int
read_raw_val (const char* s, long * vptr)
{
    char* trash;
    long v;

    errno = 0;    /* To distinguish success/failure after call */
    if (*s == 'x' || *s == 'X')
        v = strtol (++s, &trash, 16);
    else {
        if (*s == '#')
            s++;
        v = strtol (s, &trash, 10);
    }

    /* check for strtol() failure */
    if ( s == trash
            || (errno == ERANGE && (v == LONG_MAX || v == LONG_MIN))
            || (errno != 0 && v == 0)) {
        show_error("cant recognise the number (%s)\n", s);
        num_errors++;
        return -1;
    }

    if (0x10000 > v && 0x8000 <= v)
        v |= -65536L;   /* handles 64-bit longs properly */

    *vptr = v;
    return 0;
}

/* Read value.
   Format (signed/unsigned) is irrelevant as long value fits in size specified
   */
static int
read_val (const char* s, int* vptr, int bits)
{
    long v;
    if (read_raw_val(s, &v))
	return -1;

    if (v < -(1L << (bits - 1)) || v >= (1L << bits)) {
        show_error("constant outside of allowed range\n");
        num_errors++;
        return -1;
    }
    if ((v & (1UL << (bits - 1))) != 0)
        v |= ~((1UL << bits) - 1);
    *vptr = v;
    return 0;
}

/* Read value.
   It must fit into signed integer of specified size.
   */
static int
read_signed_val (const char* s, int* vptr, int bits)
{
    long v;
    if (read_raw_val(s, &v))
	return -1;

    if (v < -(1L << (bits - 1)) || v >= (1L << (bits-1))) {
        show_error("constant outside of allowed range\n");
        num_errors++;
        return -1;
    }
    if ((v & (1UL << (bits - 1))) != 0)
        v |= ~((1UL << bits) - 1);
    *vptr = v;
    return 0;
}

/* Read value.
   It must fit into unsigned integer of specified size.
   */
static int
read_unsigned_val (const char* s, int* vptr, int bits)
{
    long v;
    if (read_raw_val(s, &v))
	return -1;

    if (v < 0) {
	show_error("unsigned constant expected\n");
	num_errors++;
	return -1;
    }

    if (v >= (1L << (bits))) {
        show_error("constant outside of allowed range\n");
        num_errors++;
        return -1;
    }

    *vptr = v;
    return 0;
}

static void
write_value (int val, int dbg)
{
    static int old_line = -1;
    static int old_loc = -1;
    char out[2];
    char bits[16+1];
    int i;
    int this_loc = code_loc;

    code_loc = (code_loc + 1) & 0xFFFF;
    if (code_loc == 0) {
	show_error("More code than available address space.\n");
	exit(1);
    }	
    if (pass == 1)
        return;
        
    /* FIXME: just htons... */
    out[0] = (val >> 8);
    out[1] = (val & 0xFF);
    fwrite (out, 2, 1, objout);
    if (!saw_orig) { /* This is first word (offset, not an instruction) */
        /*
         * Listing file
         */
        fprintf(lstout,   " Addr: OPCODE   LineNo: Source\n");
        /* 
         * VHDL constants file
         */
        if (this_loc != 0) {
            fprintf(vcout, "%d to 16#%x# => X\"0000\", -- addr 0x%04x to 0x%04x\n", 
                0, this_loc-1, 0, this_loc-1);
        }
        vcout_line_addr = val;
    } else { /* don't write the offset (the first word) into bin file */
        /*
         * Listing file
         */
	{
            const char * label = last_label ? last_label : "";
            const char * cmd = last_cmd ? last_cmd : "";
            char * rest = last_cmd ? yytext : "\n";
            fprintf(lstout, "x%04x: x%04x    %6d: %16s %s %s", this_loc, val, line_num, label, cmd, rest);
        }   
         
        /*
         * debug information
         */
	/* assembly line numbers */
        if (dbg && old_line != line_num && old_loc != this_loc) {
            fprintf (dbgout, "@0:%d:%.4x:%.4x\n", line_num, this_loc, this_loc);
            old_line = line_num;
            old_loc = this_loc;
        }
	/* C line numbers (output after to override assembly line numbers) */
	if (dbg && dbg_line_state==DBG_LINE_PARSED) {
            dbg_line_start_addr = this_loc;
	    dbg_line_state = DBG_LINE_STARTED;
	}

    	/*
         * VHDL constants file
         */
    	if (this_loc % 8 == 7) {
	    	fprintf(vcout, "X\"%04x\",  -- addr 0x%04x to 0x%04x\n", val, vcout_line_addr, this_loc);
	    	vcout_line_addr = this_loc+1;
	    } else
	    	fprintf(vcout, "X\"%04x\", ", val);
	    	
        /*
         * Bits file
         */
        bits[0] = '\n';
        for (i=1; i <= 16; i++)
            bits[i] = (val & (1U << (16-i))) ? '1' : '0';

        /* Xilinx doesn't like trailing end of line, so we avoid it at the end */
        if (this_loc > code_orig)
            fwrite (bits, 17, 1, binout);
        else
            fwrite (bits+1, 16, 1, binout);
    }
    
}

static char*
sym_name (const char* name)
{
    char* local = strdup (name);
    char* cut;

    /* Not fast, but no limit on label length...who cares? */
    for (cut = local; *cut != 0 && !isspace((unsigned char)*cut) && *cut != ':' && *cut != ';'; cut++);
    *cut = 0;

    return local;
}

static int
find_label (const char* optarg, int bits)
{
    char* local;
    symbol_t* label;
    int limit, value;

    if (pass == 1)
        return 0;

    local = sym_name (optarg);
    label = find_symbol (local, NULL);
    if (label != NULL) {
	value = label->addr;
	if (bits != 16) { /* Everything except 16 bits is PC-relative. */
	    limit = (1L << (bits - 1));
	    value -= code_loc + 1;
	    if (value < -limit || value >= limit) {
	        show_error("label \"%s\" at distance %d (allowed "
			 "range is %d to %d)\n", local, value,
			 -limit, limit - 1);
	        goto bad_label;
	    }
	    return value;
	}
	free (local);
        return label->addr;
    }
    show_error("unknown label \"%s\"\n", local);

bad_label:
    num_errors++;
    free (local);
    return 0;
}

static void
handle_debug (debug_stab_t stype, char* opstr)
{
    char* tail;

    if (pass == 1)
        return;

    tail = opstr + strlen(opstr)-1;
    while (*tail == '\r' || *tail == '\n') {
	*(tail--) = 0;
    }

    switch(stype) {
        case D_FILE:
	    /* Just copy the value */
	    //fprintf (dbgout, "#%s\n", yytext);
	    fprintf (dbgout, "#%s\n", opstr);
	    break;

        case D_LINE:
            if (dbg_line_state==DBG_LINE_STARTED) {
                /* finish previous line */
                fprintf (dbgout, "@%s:%.4x:%.4x\n", dbg_line_info, dbg_line_start_addr, code_loc-1);
            }
            dbg_line_state = DBG_LINE_PARSED;
	    dbg_line_info[sizeof(dbg_line_info)-1] = 0;
	    strncpy(dbg_line_info, yytext, sizeof(dbg_line_info)-1);
	    break;

        case D_LINE_END:
            if (dbg_line_state==DBG_LINE_STARTED) {
                /* finish previous line */
                fprintf (dbgout, "@%s:%.4x:%.4x\n", dbg_line_info, dbg_line_start_addr, code_loc-1);
            }
            dbg_line_state = DBG_LINE_NONE;
	    break;

	case D_TYPE:
	    /* Just copy the value */
	    fprintf (dbgout, "T %s\n", opstr);
	    break;

	case D_SYMBOL:
	    /* Just copy the value */
	    fprintf (dbgout, "S %s\n", opstr);
	    break;

	case D_BLOCK:
	    {
	    int is_block_start = opstr[0] == 'S';
	    /* Just copy the value */

	    fprintf (dbgout, "B %s:%.4x\n", opstr, (is_block_start) ? code_loc : code_loc-1);
	    break;
	    }
    }

}

static void 
generate_instruction (operands_t operands, const char* opstr)
{
    int val, r1, r2, r3;
    const char* o1;
    const char* o2;
    const char* o3;
    const char* str;

    if ((op_format_ok[inst.op] & (1UL << operands)) == 0) {
	bad_operands ();
	return;
    }

    /* o1 = start of op1 */
    o1 = opstr;
    while (isspace((unsigned char)*o1)) o1++;	 /* skip spaces before op1 */

    /* o2 = start of op2 */
    o2=o1; while (*o2!=',' && !isspace((unsigned char)*o2)) o2++; /* o2 = to the end of op1 */
    while (isspace((unsigned char)*o2)) o2++;	 /* skip spaces before ',' */
    if (*o2==',') o2++;
    while (isspace((unsigned char)*o2)) o2++;	 /* skip spaces before op2 */

    /* o3 = start of op3 */
    o3=o2; while (*o3!=',' && !isspace((unsigned char)*o3)) o3++; /* o3 = to the end of op2 */
    while (isspace((unsigned char)*o3)) o3++;	 /* skip spaces before ',' */
    if (*o3==',') o3++;
    while (isspace((unsigned char)*o3)) o3++;	 /* skip spaces before op3 */

    if (inst.op == OP_ORIG) {
	if (saw_orig == 0) {
	    if (read_val (o1, &code_loc, 16) == -1)
		/* Pick arbitrary value, to continue input processing to report other errors.
                   The num_errors variable will prevent code generation. */
		code_loc = 0x3000; 
	    else {
                code_orig = code_loc;        /* remember orig to calculate size of code block */
	        write_value (code_loc, 0);
		code_loc--; /* Starting point doesn't count as code. */
	    }
	    saw_orig = 1;
	} else if (saw_orig == 1) {
	    show_error("multiple .ORIG directives found\n");
	    saw_orig = 2;
	}
	new_inst_line ();
	return;
    }
    if (saw_orig == 0) {
	show_error("instruction appears before .ORIG\n");
	num_errors++;
	new_inst_line ();
	saw_orig = 2;
	return;
    }
    if ((pre_parse[operands] & PP_R1) != 0)
        r1 = o1[1] - '0';
    if ((pre_parse[operands] & PP_R2) != 0)
        r2 = o2[1] - '0';
    if ((pre_parse[operands] & PP_R3) != 0)
        r3 = o3[1] - '0';
    if ((pre_parse[operands] & PP_I2) != 0)
        (void)read_signed_val (o2, &val, 9);
    if ((pre_parse[operands] & PP_L2) != 0)
        val = find_label (o2, 9);

    switch (inst.op) {
	/* Generate real instruction opcodes. */
	case OP_ADD:
	    if (operands == O_RRI) {
	    	/* Check or read immediate range (error in first pass
		   prevents execution of second, so never fails). */
	        (void)read_signed_val (o3, &val, 5);
		write_value (0x1020 | (r1 << 9) | (r2 << 6) | (val & 0x1F), 1);
	    } else
		write_value (0x1000 | (r1 << 9) | (r2 << 6) | r3, 1);
	    break;
	case OP_AND:
	    if (operands == O_RRI) {
	    	/* Check or read immediate range (error in first pass
		   prevents execution of second, so never fails). */
	        (void)read_signed_val (o3, &val, 5);
		write_value (0x5020 | (r1 << 9) | (r2 << 6) | (val & 0x1F), 1);
	    } else
		write_value (0x5000 | (r1 << 9) | (r2 << 6) | r3, 1);
	    break;
	case OP_BR:
	    if (operands == O_I)
	        (void)read_signed_val (o1, &val, 9);
	    else /* O_L */
	        val = find_label (o1, 9);
	    write_value (inst.ccode | (val & 0x1FF), 1);
	    break;
	case OP_JMP:
	    write_value (0xC000 | (r1 << 6),1);
	    break;
	case OP_JSR:
	    if (operands == O_I)
	        (void)read_signed_val (o1, &val, 11);
	    else /* O_L */
	        val = find_label (o1, 11);
	    write_value (0x4800 | (val & 0x7FF), 1);
	    break;
	case OP_JSRR:
	    write_value (0x4000 | (r1 << 6), 1);
	    break;
	case OP_LD:
	    write_value (0x2000 | (r1 << 9) | (val & 0x1FF), 1);
	    break;
	case OP_LDI:
	    write_value (0xA000 | (r1 << 9) | (val & 0x1FF), 1);
	    break;
	case OP_LDR:
	    (void)read_signed_val (o3, &val, 6);
	    write_value (0x6000 | (r1 << 9) | (r2 << 6) | (val & 0x3F), 1);
	    break;
	case OP_LEA:
	    write_value (0xE000 | (r1 << 9) | (val & 0x1FF), 1);
	    break;
	case OP_NOT:
	    write_value (0x903F | (r1 << 9) | (r2 << 6), 1);
	    break;
	case OP_RTI:
	    write_value (0x8000, 1);
	    break;
	case OP_ST:
	    write_value (0x3000 | (r1 << 9) | (val & 0x1FF), 1);
	    break;
	case OP_STI:
	    write_value (0xB000 | (r1 << 9) | (val & 0x1FF), 1);
	    break;
	case OP_STR:
	    (void)read_signed_val (o3, &val, 6);
	    write_value (0x7000 | (r1 << 9) | (r2 << 6) | (val & 0x3F), 1);
	    break;
	case OP_TRAP:
	    (void)read_unsigned_val (o1, &val, 8);
	    write_value (0xF000 | (val & 0xFF),1);
	    break;

	/* Generate trap pseudo-ops. */
	case OP_GETC:  
            write_value (0xF020,1);
            break;
	case OP_HALT:
            write_value (0xF025,1);
            break;
	case OP_IN:
            write_value (0xF023,1);
            break;
	case OP_OUT:
            write_value (0xF021,1);
            break;
	case OP_PUTS:
            write_value (0xF022,1);
            break;
	case OP_PUTSP:
            write_value (0xF024,1);
            break;

	/* Generate non-trap pseudo-ops. */
    	case OP_FILL:
	    if (operands == O_I) {
		(void)read_val (o1, &val, 16);
		val &= 0xFFFF;
	    } else /* O_L */
		val = find_label (o1, 16);
	    write_value (val,0);
    	    break;
	case OP_RET:   
	    write_value (0xC1C0,1); 
	    break;
	case OP_STRINGZ:
	    /* We must count locations written in pass 1;
	       write_value squashes the writes. */
	    for (str = o1 + 1; str[0] != '\"'; str++) {
		if (str[0] == '\\') {
		    switch (str[1]) {
			case 'a': write_value ('\a', 0); str++; break;
			case 'b': write_value ('\b', 0); str++; break;
			case 'e': write_value ('\e', 0); str++; break;
			case 'f': write_value ('\f', 0); str++; break;
			case 'n': write_value ('\n', 0); str++; break;
			case 'r': write_value ('\r', 0); str++; break;
			case 't': write_value ('\t', 0); str++; break;
			case 'v': write_value ('\v', 0); str++; break;
			case '\\': write_value ('\\', 0); str++; break;
			case '\"': write_value ('\"', 0); str++; break;
			/* FIXME: support others too? */
			default: write_value (str[1],0); str++; break;
		    }
		} else {
                    if (str[0] == '\r') {
                        if (str[1] == '\n') 
                            str++;
                        line_num++;
                    } else if (str[0] == '\n')
		        line_num++;
		    write_value (*str, 0);
		}
		last_cmd = NULL;	// To disable the source display for the next values
	    }
	    write_value (0, 0);
	    break;
	case OP_BLKW:
	    (void)read_val (o1, &val, 16);
	    val &= 0xFFFF;
	    while (val-- > 0) {
	        write_value (0x0000, 0);
		last_cmd = NULL;	// To disable the source display for the next values
	    }
	    break;
	
	/* Handled earlier or never used, so never seen here. */
	case OP_NONE:
        case OP_ORIG:
        case OP_END:
	case NUM_OPS:
	    break;

/************************************************/
/*		DTU extensions			*/
    	/* Shift Left Logical */
	case OP_SLL:
	    if (operands == O_RRI) {
	        (void)read_unsigned_val (o3, &val, 4);
		write_value (0x1010 | (r1 << 9) | (r2 << 6) | (val & 0xF), 1);
	    } else
		write_value (0xD000 | (r1 << 9) | (r2 << 6) | r3, 1);
	    break;

	/* Shift Right Arithmetic */
	case OP_SRA:
	    if (operands == O_RRI) {
	        (void)read_unsigned_val (o3, &val, 4);
		write_value (0x5010 | (r1 << 9) | (r2 << 6) | (val & 0xF), 1);
	    } else
		write_value (0xD000 | (r1 << 9) | (r2 << 6) | ((OP_SRA-OP_SLL) << 3) | r3, 1);
	    break;

	/* Divide */
	case OP_DIV:
	    write_value (0xD000 | (r1 << 9) | (r2 << 6) | ((OP_DIV-OP_SLL) << 3) | r3, 1);
	    break;

	/* Remainder */
	case OP_MOD:
	    write_value (0xD000 | (r1 << 9) | (r2 << 6) | ((OP_MOD-OP_SLL) << 3) | r3, 1);
	    break;

	/* Multiply */
	case OP_MUL:
	    write_value (0xD000 | (r1 << 9) | (r2 << 6) | ((OP_MUL-OP_SLL) << 3) | r3, 1);
	    break;

	/* pseudo-ops */
	case OP_NOP:
            write_value (0x0000, 1);
            break;

        /* directives */
        case OP_BLKWTO:
	    (void)read_val (o1, &val, 16);
	    val &= 0xFFFF;
            if (code_loc > val) {
                show_error("requested address is in the past\n");
                num_errors++;
            } else {
	        while (code_loc < val) {
                    write_value (0x0000, 0);
	            last_cmd = NULL;	// To disable the source display for the next values
                }
            }
	    break;
/*						*/
/************************************************/
    }
    new_inst_line ();
}

static void 
parse_ccode (const char* ccstr)
{
    
    static char br_with_cond[6];
    snprintf(br_with_cond, 6, "BR%s", ccstr);
    last_cmd = br_with_cond; // for listing output

    if (*ccstr == 'N' || *ccstr == 'n') {
	inst.ccode |= CC_N;
        ccstr++;
    }
    if (*ccstr == 'Z' || *ccstr == 'z') {
	inst.ccode |= CC_Z;
        ccstr++;
    }
    if (*ccstr == 'P' || *ccstr == 'p')
	inst.ccode |= CC_P;

    /* special case: map BR to BRnzp */
    if (inst.ccode == CC_)
        inst.ccode = CC_P | CC_Z | CC_N;
}

static void
found_label (const char* lname) 
{
    if (last_label) free(last_label);
    last_label = sym_name (lname);

    if (pass == 1) {
	if (saw_orig == 0) {
	    show_error("label appears before .ORIG\n");
	    num_errors++;
	} else if (add_symbol (last_label, code_loc, 0) == -1) {
	    show_error("label %s has already appeared\n", last_label);
	    num_errors++;
	} else {
	    fprintf (symout, "//\t%-16s  %04X\n", last_label, code_loc);
	    fprintf (dbgout, "!%04x:%s\n", code_loc, last_label);
        }
    }
}

