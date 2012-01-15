#include "lc3_stab.h"
//#include "stab.h"

int lc3_stabFileId = 0;
static char *currentfile;

static int ntypes = 0;
static void asgncode(Type, int);
static void dbxout(Type);
static int dbxtype(Type);
static int emittype(Type, int, int);

/*

  Type information
  ----------------
  void			v
  int (signed/unsgn)	i/I
  char (textual)	c
  short (signed/unsgn)	h/H
  long (signed/unsgn)	l/L
  pointer		*<baseT>
  enum			e;<name>:<val>,<name2>:<val2>
  array			a<baseT>,<length>,<baseSize/padding>
  struct		s<size>;<name>:<type>,<bitOffset>,<bitSize>;<name2>:<type2>,<bitOffset2>,<bitSize2>
  union			u	// save as struct
  function		f<returnT>;<pName>:<pType>,<location/frameOffset>;<pName2>:<pType2>,<location/frameOffset2>

  forward declaration   x<Struct/Union>

  variable symbols:
  ----------
	G	Global variable
 	S	File scope static (file internal)
	s	Function scope static (static local)
	l	Local variable
	p	Parameter
  function symbols:
  ----------
  	F	Global function
	f	Local (static) function

  .debug block S:<functionName>:<level>		Start of the block
  .debug block E:<functionName>:<level>		End of the block

*/

/* lc3_stabblock - output a stab entry for '{' or '}' at level lev */
void lc3_stabblock(int brace, int lev, Symbol *p) {
	print(".debug block %c:%s:%d\n",brace == '{' ? 'S' : 'E', cfunc->x.name,  lev);
	if (brace == '{')
		while (*p)
			lc3_stabsym(*p++);
}



/* lc3_stabinit - initialize stab output */
void lc3_stabinit(char *file, int argc, char *argv[]) {
	if (file) {
		currentfile = file;
		lc3_stabFileId++;
		print(".debug file %d:%s\n", lc3_stabFileId, currentfile);
	}
#if 1
	dbxtype(inttype);
	dbxtype(chartype);
//	dbxtype(doubletype);
//	dbxtype(floattype);
//	dbxtype(longdouble);
	dbxtype(longtype);
	dbxtype(longlong);
	dbxtype(shorttype);
	dbxtype(signedchar);
	dbxtype(unsignedchar);
	dbxtype(unsignedlong);
	dbxtype(unsignedlonglong);
	dbxtype(unsignedshort);
	dbxtype(unsignedtype);
	dbxtype(voidtype);
//	foreach(types, GLOBAL, (Closure)stabtype, NULL);
#endif
}

/* lc3_stabline - emit stab entry for source coordinate *cp */
void lc3_stabline(Coordinate *cp) {
	if (cp->file && cp->file != currentfile) {
		//fprintf(stderr, ".before: %s, now: %s, cmp: %d\n", currentfile, cp->file, strcmp(cp->file, currentfile));
		currentfile = cp->file;
		lc3_stabFileId++;
		print(".debug file %d:%s\n", lc3_stabFileId, currentfile);
	}
	print(".debug line %d:%d\n", lc3_stabFileId, cp->y);
}


/* asgncode - assign type code to ty */
static void asgncode(Type ty, int lev) {
	if (ty->x.marked || ty->x.typeno)
		return;
	ty->x.marked = 1;
	switch (ty->op) {
	case VOLATILE: case CONST: case VOLATILE+CONST:
		asgncode(ty->type, lev);
		ty->x.typeno = ty->type->x.typeno;
		break;
	case POINTER: case FUNCTION: case ARRAY:
		asgncode(ty->type, lev + 1);
		/* fall thru */
	case VOID: case INT: case UNSIGNED: case FLOAT:
		if (ty->x.typeno == 0){
			ty->x.typeno = ++ntypes;
			//fprintf(stderr, "type as i: %p %d\n", ty, ntypes);
		}
		break;
	case STRUCT: case UNION: {
		Field p;
		for (p = fieldlist(ty); p; p = p->link)
			asgncode(p->type, lev + 1);
		/* fall thru */
	case ENUM:
		if (ty->x.typeno == 0){
			ty->x.typeno = ++ntypes;
			//fprintf(stderr, "type as: %p %d\n", ty, ntypes);
		}
		if (lev > 0 && (*ty->u.sym->name < '0' || *ty->u.sym->name > '9'))
			// FixMe: commented
			dbxout(ty);
		break;
		}
	default:
		assert(0);
	}
}

/* dbxout - output .stabs entry for type ty */
static void dbxout(Type ty) {
	ty = unqual(ty);
	if (!ty->x.printed) {
		int col = 0;
		//if (ty->u.sym && !(isfunc(ty) || isarray(ty) || isptr(ty)))
		//	print("%s", ty->u.sym->name);
		int tc = emittype(ty, 0, col);
		if (ty->u.sym && !(isfunc(ty) || isarray(ty) || isptr(ty))) {
			print("; .debug typedef %s:%c%d\n", ty->u.sym->name,
			       	isstruct(ty) || isenum(ty) ? 'T' : 't', tc);
//			       	isstruct(ty) || isenum(ty) ? 'T' : 't', dbxtype(ty));
		}
	}
}

/* dbxtype - emit a stabs entry for type ty, return type code */
static int dbxtype(Type ty) {
	asgncode(ty, 0);
	dbxout(ty);
	return ty->x.typeno;
}

/*
 * emittype - return ty's type number, emitting its definition if necessary.
 */
static int emittype(Type ty, int lev, int col) {
	int tc = ty->x.typeno;
	int outSize = 64;
	char *outBuf = malloc(outSize);
	char *out = outBuf;

	if(ty->x.printed) {
		if(tc == 0) fprintf(stderr, "WARNING: Skipping type: %d, %d\n", lev, col);
		return tc;
	}

	/* don't create separate types for const/volatile qualifiers */
	if (isconst(ty) || isvolatile(ty)) {
		tc = emittype(ty->type, lev, col);
		ty->x.typeno = ty->type->x.typeno;
		ty->x.printed = 1;
		return tc;
	}

	asgncode(ty, 0);
	ty->x.printed = 1;
	// FixMe: Use continuations, or reallocable output buffer
	//        because structure descriptions can be arbitrary long
	// FixMe: don't emit standard types

	switch (ty->op) {
	case VOID:	/* void is defined as itself */
		*(out++) = 'v'; *(out++) = 0;
		break;
	case CHAR:
		/* This is unreachable in lcc 4.x version of the interface.
		   CHAR is encoded as INT with size==align==1 in new interface version */
		*(out++) = 'c'; *(out++) = 0;
		break;
	case INT:
		if (ischar(ty)) {
			*(out++) = 'c'; *(out++) = 0;
		} else {
			*(out++) = 'i'; *(out++) = 0;
		}
		break;
	case UNSIGNED:
		if (ischar(ty)) {
			*(out++) = 'c'; *(out++) = 0;
		} else {
			*(out++) = 'I'; *(out++) = 0;
		}
		break;
	case FLOAT:	/* float, double, long double get sizes, not ranges */
		fprintf(stderr, "Float types not supported\n");
		break;
	case POINTER:
		out += sprintf(out, "*%d", emittype(ty->type, lev + 1, col));
		break;
	case FUNCTION:
		out += sprintf(out, "f%d;", emittype(ty->type, lev + 1, col));
		// FixMe: output arguments
		break;
	case ARRAY:	/* array includes subscript as an int range */
		tc = emittype(ty->type, lev + 1, col);
		out += sprintf(out, "a%d,%d", tc,
			       (ty->size && ty->type->size) ? ty->size/ty->type->size : -1);
		break;
	case STRUCT: case UNION: {
		Field p;
		if (!ty->u.sym->defined) {
			out += sprintf(out, "x%c%s:", ty->op == STRUCT ? 's' : 'u', ty->u.sym->name);
			break;
		}
		if (lev > 0 && (*ty->u.sym->name < '0' || *ty->u.sym->name > '9')) {
			// FixMe: is this realy needed??
			ty->x.printed = 0;
			break;
		}
		out += sprintf(out, "%c%d", ty->op == STRUCT ? 's' : 'u', ty->size);
		for (p = fieldlist(ty); p; p = p->link) {
			tc = emittype(p->type, lev + 1, col);
			out += sprintf(out, ";%s:%d,", (p->name) ? p->name : "", tc);
			if (p->lsb)
				out += sprintf(out, "%d,%d", 8*p->offset +
					(IR->little_endian ? fieldright(p) : fieldleft(p)),
					fieldsize(p));
			else
				out += sprintf(out, "%d,%d", 8*p->offset, 8*p->type->size);
			/*
			if (col >= 80 && p->link) {
				print("\\\\\",%d,0,0,0\n;.stabs \"", N_LSYM);
				col = 8;
			}
			*/
		}
		break;
		}
	case ENUM: {
		Symbol *p;
		if (lev > 0 && (*ty->u.sym->name < '0' || *ty->u.sym->name > '9')) {
			ty->x.printed = 0;
			break;
		}
		out += sprintf(out, "e");
		for (p = ty->u.sym->u.idlist; *p; p++) {
			out += sprintf(out, ";%s:%d", (*p)->name, (*p)->u.value);
		}
		break;
		}
	default:
		assert(0);
	}
	tc = ty->x.typeno;
	if (tc == 0) {
		ty->x.typeno = tc = ++ntypes;
		//fprintf(stderr, "type et: %p %d\n", ty, ntypes);
	}
	print(".debug type %d=%s\n", tc, outBuf);
	return tc;
}
/* lc3_stabsym - output a stab entry for symbol p */
void lc3_stabsym(Symbol p) {
	int code, tc, sz = p->type->size;

    if (p == cfunc && IR->stabline)
        (*IR->stabline)(&p->src);

	if (p->generated || p->computed)
		return;
	if (isfunc(p->type)) {
		//print(".debug function %s(%s):%c%d\n", p->name, p->x.name,
		print(".debug symbol %c%d:%s:%s\n",
			p->sclass == STATIC ? 'f' : 'F', dbxtype(freturn(p->type)),
			p->name, p->x.name);
			//p->sclass == STATIC ? 'f' : 'F', emittype(freturn(p->type), 0, 0));
		return;
	}

	// FixMe: check wants_argb
	if (!IR->wants_argb && p->scope == PARAM && p->structarg) {
		assert(isptr(p->type) && isstruct(p->type->type));
		tc = dbxtype(p->type->type);
		sz = p->type->type->size;
	} else
		tc = dbxtype(p->type);
	if (p->sclass == AUTO && p->scope == GLOBAL || p->sclass == EXTERN) {
		print(".debug symbol G%d:%s:%s\n", tc, p->name, p->x.name);
	} else if (p->sclass == STATIC) {
		print(".debug symbol %c%d:%s:%s\n",
				p->scope == GLOBAL ? 'S' : 's',
				tc, p->name, p->x.name);
		//return;
	} else if (p->sclass == REGISTER) {
		fprintf(stderr, "Register storage class not supported\n");
		assert(0);
		return;
	} else if (p->scope == PARAM) {
		print(".debug symbol p%d:%s:%s\n", tc, p->name, p->x.name);
	} else if (p->scope >= LOCAL) {
		print(".debug symbol l%d:%s:%s\n", tc, p->name, p->x.name);
	} else
		assert(0);
	//print("%d,%s\n", tc,
	//	p->scope >= PARAM && p->sclass != EXTERN ? p->x.name : "0");
}



// These are not used for debug info generation, but rather as general function
// common to both lc3 targets.

#ifndef MAX
#define MAX(x, y) ((x)<(y) ? (y):(x))
#endif
#ifndef MIN
#define MIN(x, y) ((x)<(y) ? (x):(y))
#endif

int calculate_address(int dstReg, int baseReg_, int offset, int withLDR, FILE *output) {
	int baseReg = baseReg_;
	int sum = 0;
	int ldrOffset;

	if (offset > 0) {
		ldrOffset = withLDR ? MIN(offset,31) : 0;
		offset -= ldrOffset;
		if (offset <= 15*3) { /* Is it cheaper to calculate offset */
			while (offset) {
				int addOffset = MIN(offset, 15);
				offset -= addOffset; sum += addOffset;
				fprintf(output, "ADD R%d, R%d, #%-4d	; R%d[%d]\n", dstReg, baseReg, addOffset, baseReg_, sum);
				baseReg = dstReg;
			}
		} else {
		}
	} else if (offset < 0) {
		ldrOffset = withLDR ? MAX(offset,-32) : 0;
		offset -= ldrOffset;
		if (offset >= -16*3) { /* Is it cheaper to calculate offset */
			while (offset) {
				int addOffset = MAX(offset, -16);
				offset -= addOffset; sum += addOffset;
				fprintf(output, "ADD R%d, R%d, #%-4d	; R%d[%d]\n", dstReg, baseReg, addOffset, baseReg_, sum);
				baseReg = dstReg;
			}
		}
	} else if (!withLDR) {
		fprintf(output, "ADD R%d, R%d, #%-4d\n", dstReg, baseReg, offset);
	}

	if (offset) {	// It was not efficient to calculate the offset, let's load it from the constant
		offset += ldrOffset;
		ldrOffset = 0;
		fprintf(output, "BR #1\n"
				".FILL #%d\n"
				"LD R%d, #-2\n"
				"ADD R%d, R%d, R%d\n", offset+ldrOffset, dstReg, dstReg, baseReg, dstReg);
		baseReg = dstReg;
		sum = offset;
	}

	if (withLDR) { /* need to also load the value from the calculated address */
		fprintf(output, "LDR R%d, R%d, #%-4d	; R%d[%d]\n", dstReg, baseReg, ldrOffset, baseReg_, sum+ldrOffset);
	}
}

