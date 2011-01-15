/*
 * Status:
 *	Workaround known
 * Description:
 *	Constant folding. Fails to calculate correct folded value.
 *	  1. Unsigned constants become truncated at 8'th bit. This happens
 *	because lc3 has sizeof(int)==1, and lcc truncates unsigned constants
 *	at 8*sizeof, to preserve semantics.
 *	  2. Bitwise complement (BCOM) also fails the same way with signed constants.
 *	From the source it seams that bitwise and (BAND) might also be affected.
 * Workaround:
 *	1. Don't use constant expressions. Assign constant and compute
 *	   expression in separate statements.
 *	2. Don't use unsigned constants. Express the same value as signed
 *	   (possibly negative) number. (Won't help with BCOM, BAND)
 * Source:
 *	simplify() in src/simp.c:
 *		  see:  l->u.v.i & ones(8*ty->size)
 *		  in:
 *		case CVI+U:
 *		case CVU+U:
 *		case BAND+U:
 *		case BAND+I:
 *		case BCOM+U:
 *		case BCOM+I:
 * Solution idea:
 *	Because sizeof(int) == sizeof(char), we can't infer type from ty->size.
 *	The ranges should be calculated according the ty matching Type variables
 *	like: unsignedtype, unsignedlong, ...  (from src/types.c)
 *	There are probably more related problems waiting to emerge
 *	  "grep '[.>]size\>'  *.c *.h" returns quite some number of uses of
 *	ty->size. Maybe it might be good idea to introduce new field in Type
 *	struct to distinguish betwean sizeof() size and valid range.
 */

#include <stdio.h>

#define UNSIGNED_CONST 0x00ffu
//
// Also doesn't work with any other constant bigger than 255
//
// #define UNSIGNED_CONST 0x0100u
// #define UNSIGNED_CONST 0x1000u
// #define UNSIGNED_CONST 0x8000u
//
//
// But works fine if signed constants are used
//
// #define UNSIGNED_CONST 0x0100u
// #define UNSIGNED_CONST 0x1000u
// #define UNSIGNED_CONST 0x8000u
//
#define SIGNED_CONST 0x02ff

int main() {
	/*
	 * Unsigned truncation problem
	 */
	int i = UNSIGNED_CONST+1;

	printf("1. Badd: %d %x \n", i, i);

	i = UNSIGNED_CONST;
	i += 1;
	printf("1. Good: %d %x \n", i, i);

	/*
	 * Unsigned truncation problem
	 */
	i = ~SIGNED_CONST;

	printf("2. Badd: %d %x \n", i, i);

	i = SIGNED_CONST;
	i = ~i;
	printf("2. Good: %d %x \n", i, i);


	return 0;
}
