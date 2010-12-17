/* *-*- C -*-* */
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
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

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
    OP_BLKW, OP_END, OP_ORIG, OP_BLKWTO,

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
    ".BLKW", ".END", ".ORIG", ".BLKWTO"
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
    0x040, /* .ORIG: I format only         */
    0x040  /* .BLKWTO: I format only       */
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

typedef struct symbol_t symbol_t;
struct symbol_t {
    char* name;
    int addr;
    symbol_t* next_with_hash;
#ifdef MAP_LOCATION_TO_SYMBOL
    symbol_t* next_at_loc;
#endif
};

#define SYMBOL_HASH 997

static int pass, line_num, num_errors, saw_orig, code_loc;
static inst_t inst;
static FILE* dbgout;
static FILE* objout;
static FILE* hexout;

static void new_inst_line ();
static void bad_operands ();
static void bad_line ();
static void parse_ccode (const char*);
static void generate_instruction (operands_t, const char*);
static void found_label (const char* lname);

static int symbol_hash (const char* symbol);
static int add_symbol (const char* symbol, int addr, int dup_ok);
static symbol_t* find_symbol (const char* symbol, int* hptr);
#ifdef MAP_LOCATION_TO_SYMBOL
static void remove_symbol_at_addr (int addr);
#endif

symbol_t* lc3_sym_hash[SYMBOL_HASH];
#ifdef MAP_LOCATION_TO_SYMBOL
symbol_t* lc3_sym_names[65536];
#endif

%}

/* condition code specification */
CCODE    [Nn]?[Zz]?[Pp]?

/* operand types */
REGISTER [rR][0-7]
HEX      [xX][-]?[0-9a-fA-F]+
DECIMAL  [#]?[-]?[0-9]+
IMMED    {HEX}|{DECIMAL}
/*
 * Made same as windows assembler/
LABEL    [A-Za-z][A-Za-z_0-9]*
 */
LABEL    [_A-Za-z][A-Za-z_0-9]*
/* strings with no carriage returns */
STRING   \"([^"\n\r\\]*\\[^\n\r])*[^"\n\r\\]*\" 

/* strings with carriage returns 
STRLINE  ([^"\n\r\\]*\\[^\n\r])*[^"\n\r\\]*

/* This needs to move down into the next section... 
\"	                  {BEGIN (ls_string);}
<ls_string>STRLINE        {yymore ();}
<ls_string>[\\]?[\n][\r]? {line_num++; yymore ();}
<ls_string>\"             {}
*/

/* operand and white space specification */
SPACE    [ \t]
OP_SEP   {SPACE}*[, \t]{SPACE}*
COMMENT  [;][^\n\r]*
ENDLINE  {SPACE}*{COMMENT}?\n\r?

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
O_     {ENDLINE}

/* need to define YY_INPUT... */

/* exclusive lexing states to read operands and eat garbage lines */
%x ls_operands ls_garbage

%%

    /* rules for real instruction opcodes */
ADD       {inst.op = OP_ADD;   BEGIN (ls_operands);}
AND       {inst.op = OP_AND;   BEGIN (ls_operands);}
BR{CCODE} {inst.op = OP_BR;    parse_ccode (yytext + 2); BEGIN (ls_operands);}
JMP       {inst.op = OP_JMP;   BEGIN (ls_operands);}
JSRR      {inst.op = OP_JSRR;  BEGIN (ls_operands);}
JSR       {inst.op = OP_JSR;   BEGIN (ls_operands);}
LDI       {inst.op = OP_LDI;   BEGIN (ls_operands);}
LDR       {inst.op = OP_LDR;   BEGIN (ls_operands);}
LD        {inst.op = OP_LD;    BEGIN (ls_operands);}
LEA       {inst.op = OP_LEA;   BEGIN (ls_operands);}
NOT       {inst.op = OP_NOT;   BEGIN (ls_operands);}
RTI       {inst.op = OP_RTI;   BEGIN (ls_operands);}
STI       {inst.op = OP_STI;   BEGIN (ls_operands);}
STR       {inst.op = OP_STR;   BEGIN (ls_operands);}
ST        {inst.op = OP_ST;    BEGIN (ls_operands);}
TRAP      {inst.op = OP_TRAP;  BEGIN (ls_operands);}

    /* rules for trap pseudo-ols */
GETC      {inst.op = OP_GETC;  BEGIN (ls_operands);}
HALT      {inst.op = OP_HALT;  BEGIN (ls_operands);}
IN        {inst.op = OP_IN;    BEGIN (ls_operands);}
OUT       {inst.op = OP_OUT;   BEGIN (ls_operands);}
PUTS      {inst.op = OP_PUTS;  BEGIN (ls_operands);}
PUTSP     {inst.op = OP_PUTSP; BEGIN (ls_operands);}

    /* rules for non-trap pseudo-ops */
\.FILL    {inst.op = OP_FILL;  BEGIN (ls_operands);}
RET       {inst.op = OP_RET;   BEGIN (ls_operands);}
\.STRINGZ {inst.op = OP_STRINGZ; BEGIN (ls_operands);}

    /* rules for directives */
\.BLKW    {inst.op = OP_BLKW;  BEGIN (ls_operands);}
\.END     {return 0;}
\.ORIG    {inst.op = OP_ORIG;  BEGIN (ls_operands);}
\.BLKWTO  {inst.op = OP_BLKWTO; BEGIN (ls_operands);}

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
<ls_operands>[^\n\r]*<ENDLINE> {bad_operands (); BEGIN (0);}
{O_RRR}|{O_RRI}|{O_RR}|{O_RI}|{O_RL}|{O_R}|{O_I}|{O_S} {bad_operands ();}

. {BEGIN (ls_garbage);}
<ls_garbage>[^\n\r]*{ENDLINE} {bad_line (); BEGIN (0);}

%%

int lc3_asm (int argc, char** argv)
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

    /* Open input file. */
    if ((lc3in = fopen (fname, "r")) == NULL) {
        fprintf (stderr, "Could not open %s for reading.\n", fname);
	return 2;
    }

    /* Open output files. */
    strcpy (ext, ".obj");
    if ((objout = fopen (fname, "w"))) fclose(objout);
    chmod(fname, 0755);
    if ((objout = fopen (fname, "w")) == NULL) {
        fprintf (stderr, "Could not open %s for writing.\n", fname);
	return 2;
    }
    strcpy (ext, ".hex");
    if ((hexout = fopen (fname, "w")) == NULL) {
        fprintf (stderr, "Could not open %s for writing.\n", fname);
	return 2;
    }
    strcpy(ext, ".dbg");
    if ((dbgout = fopen (fname, "w")) == NULL) {
	 fprintf (stderr, "Could not open %s for writing.\n", fname);
	 return 2;
    }
    {
      char buf[512];
      char *cwd = getcwd(buf, sizeof(buf));
      fprintf(dbgout, "%s/%s\n", cwd, argv[1]);
    }
    pass = 1;
    line_num = 0;
    num_errors = 0;
    saw_orig = 0;
    code_loc = 0x3000;
    new_inst_line ();
    yylex ();

    if (num_errors > 0)
    	return 1;
    if (fseek (lc3in, 0, SEEK_SET) != 0) {
        perror ("fseek to start of ASM file");
	return 3;
    }
    yyrestart (lc3in);

    pass = 2;
    line_num = 0;
    num_errors = 0;
    saw_orig = 0;
    code_loc = 0x3000;
    new_inst_line ();
    yylex ();

    if (num_errors > 0)
    	return 1;
    fclose (dbgout);
    fclose (hexout);
    fclose (objout);

    return 0;
}

static void
new_inst_line () 
{
    inst.op = OP_NONE;
    inst.ccode = CC_;
    line_num++;
}

static void
bad_operands ()
{
    fprintf (stderr, "%3d: illegal operands for %s\n",
	     line_num, opnames[inst.op]);
    num_errors++;
    new_inst_line ();
}

static void 
bad_line ()
{
    fprintf (stderr, "%3d: contains unrecognizable characters\n",
	     line_num);
    num_errors++;
    new_inst_line ();
}

/* Internal function
 */
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
        fprintf (stderr, "%3d: cant recognise the number (%s)\n", line_num, s);
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
        fprintf (stderr, "%3d: constant outside of allowed range\n", line_num);
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
        fprintf (stderr, "%3d: constant outside of allowed range\n", line_num);
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
	fprintf (stderr, "%3d: unsigned constant expected\n", line_num);
	num_errors++;
	return -1;
    }

    if (v >= (1L << (bits))) {
        fprintf (stderr, "%3d: constant outside of allowed range\n", line_num);
        num_errors++;
        return -1;
    }

    *vptr = v;
    return 0;
}

static void
write_value (int val, int dbg)
{
    unsigned char out[2];
    static int old_line = -1;
    static int old_loc = -1;

    code_loc = (code_loc + 1) & 0xFFFF;
    if (pass == 1)
        return;

    /* FIXME: just htons... */
    out[0] = (val >> 8);
    out[1] = (val & 0xFF);
    fwrite (out, 2, 1, objout);
    fprintf(hexout, "%.2x%.2x\n", out[0] & 0xFF, out[1] & 0xFF);
    if (dbg && old_line != line_num && old_loc != code_loc) {
      fprintf (dbgout, "@%d:%.4x\n", line_num, code_loc - 1);
      old_line = line_num;
      old_loc = code_loc;
    }
}

static char*
sym_name (const char* name)
{
    unsigned char* local = strdup (name);
    unsigned char* cut;

    /* Not fast, but no limit on label length...who cares? */
    for (cut = local; *cut != 0 && !isspace (*cut) && *cut != ':' && *cut != ';'; cut++);
    *cut = 0;

    return local;
}

static int
find_label (const char* optarg, int bits)
{
    unsigned char* local;
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
	        fprintf (stderr, "%3d: label \"%s\" at distance %d (allowed "
			 "range is %d to %d)\n", line_num, local, value,
			 -limit, limit - 1);
	        goto bad_label;
	    }
	    return value;
	}
	free (local);
        return label->addr;
    }
    fprintf (stderr, "%3d: unknown label \"%s\"\n", line_num, local);

bad_label:
    num_errors++;
    free (local);
    return 0;
}

static void 
generate_instruction (operands_t operands, const char* opstr)
{
    int val, r1, r2, r3;
    const unsigned char* o1;
    const unsigned char* o2;
    const unsigned char* o3;
    const unsigned char* str;

    if ((op_format_ok[inst.op] & (1UL << operands)) == 0) {
	bad_operands ();
	return;
    }

    /* o1 = start of op1 */
    o1 = opstr;
    while (isspace (*o1)) o1++;	 /* skip spaces before op1 */

    /* o2 = start of op2 */
    o2=o1; while (*o2!=',' && !isspace(*o2)) o2++; /* o2 = to the end of op1 */
    while (isspace (*o2)) o2++;	 /* skip spaces before ',' */
    if (*o2==',') o2++;
    while (isspace (*o2)) o2++;	 /* skip spaces before op2 */

    /* o3 = start of op3 */
    o3=o2; while (*o3!=',' && !isspace(*o3)) o3++; /* o3 = to the end of op2 */
    while (isspace (*o3)) o3++;	 /* skip spaces before ',' */
    if (*o3==',') o3++;
    while (isspace (*o3)) o3++;	 /* skip spaces before op3 */

    if (inst.op == OP_ORIG) {
	if (saw_orig == 0) {
	    if (read_val (o1, &code_loc, 16) == -1)
		/* Pick a value; the error prevents code generation. */
		code_loc = 0x3000; 
	    else {
	        write_value (code_loc, 0);
		code_loc--; /* Starting point doesn't count as code. */
	    }
	    saw_orig = 1;
	} else if (saw_orig == 1) {
	    fprintf (stderr, "%3d: multiple .ORIG directives found\n",
		     line_num);
	    saw_orig = 2;
	}
	new_inst_line ();
	return;
    }
    if (saw_orig == 0) {
	fprintf (stderr, "%3d: instruction appears before .ORIG\n",
		 line_num);
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
	case OP_GETC:  write_value (0xF020,1); break;
	case OP_HALT:  write_value (0xF025,1); break;
	case OP_IN:    write_value (0xF023,1); break;
	case OP_OUT:   write_value (0xF021,1); break;
	case OP_PUTS:  write_value (0xF022,1); break;
	case OP_PUTSP: write_value (0xF024,1); break;

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
		} else
		    write_value (*str, 0);
	    }
	    write_value (0,0);
	    break;
	case OP_BLKW:
	    (void)read_val (o1, &val, 16);
	    val &= 0xFFFF;
	    while (val-- > 0)
	        write_value (0x0000,0);
	    break;
	case OP_BLKWTO:
	    (void)read_val (o1, &val, 16);
	    val &= 0xFFFF;
	    while (code_loc < val) {
	        write_value (0x0000,0);
	    }
	    break;
	
	/* Handled earlier or never used, so never seen here. */
	case OP_NONE:
        case OP_ORIG:
        case OP_END:
	case NUM_OPS:
	    break;
    }
    new_inst_line ();
}

static void 
parse_ccode (const char* ccstr)
{
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
    unsigned char* local = sym_name (lname);

    if (pass == 1) {
	if (add_symbol (local, code_loc, 0) == -1) {
	    fprintf (stderr, "%3d: label %s has already appeared\n", 
	    	     line_num, local);
	    num_errors++;
	} else {
	    fprintf (dbgout, "!%04x:%s\n", code_loc, local);
        }
    }

    free (local);
}

static int symbol_hash (const char* symbol)
{
    int h = 1;

    while (*symbol != 0)
	h = (h * tolower (*symbol++)) % SYMBOL_HASH;

    return h;
}

static symbol_t *find_symbol (const char* symbol, int* hptr)
{
    int h = symbol_hash (symbol);
    symbol_t* sym;

    if (hptr != NULL)
        *hptr = h;
    for (sym = lc3_sym_hash[h]; sym != NULL; sym = sym->next_with_hash)
    	if (strcasecmp (symbol, sym->name) == 0)
	    return sym;
    return NULL;
}

static int add_symbol (const char* symbol, int addr, int dup_ok)
{
    int h;
    symbol_t* sym;

    if ((sym = find_symbol (symbol, &h)) == NULL) {
	sym = (symbol_t*)malloc (sizeof (symbol_t)); 
	sym->name = strdup (symbol);
	sym->next_with_hash = lc3_sym_hash[h];
	lc3_sym_hash[h] = sym;
#ifdef MAP_LOCATION_TO_SYMBOL
	sym->next_at_loc = lc3_sym_names[addr];
	lc3_sym_names[addr] = sym;
#endif
    } else if (!dup_ok) 
        return -1;
    sym->addr = addr;
    return 0;
}


#ifdef MAP_LOCATION_TO_SYMBOL
static void remove_symbol_at_addr (int addr)
{
    symbol_t* s;
    symbol_t** find;
    int h;

    while ((s = lc3_sym_names[addr]) != NULL) {
        h = symbol_hash (s->name);
	for (find = &lc3_sym_hash[h]; *find != s; 
	     find = &(*find)->next_with_hash);
        *find = s->next_with_hash;
	lc3_sym_names[addr] = s->next_at_loc;
	free (s->name);
	free (s);
    }
}
#endif
