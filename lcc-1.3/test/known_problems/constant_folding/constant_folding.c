/*
 * Status:
 * 	Workaround known
 * Description:
 * 	Unsigned constant folding
 * 	Fails to calculate correct folded value. Unsigned constants become
 * 	truncated at 8'th bit.
 * Workaround:
 *
 */

#include <stdio.h>

#define BIG_CONST 0x00ffu
//
// Also doesn't work with any other constant bigger than 255
//
// #define BIG_CONST 0x0100u
// #define BIG_CONST 0x1000u
// #define BIG_CONST 0x8000u
//

int main() {

	int addr = BIG_CONST+1;

	printf("Badd: %d %x \n", addr, addr);

	addr = BIG_CONST;
	addr += 1;
	printf("Good: %d %x \n", addr, addr);

	return 0;
}
