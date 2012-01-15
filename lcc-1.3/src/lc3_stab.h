#ifndef _LC3_STAB_H
#define _LC3_STAB_H

#include "c.h"
#include <stdio.h>

extern int lc3_stabFileId;

void lc3_stabblock(int brace, int lev, Symbol *p);
void lc3_stabinit(char *, int, char *[]);
void lc3_stabline(Coordinate *);
void lc3_stabsym(Symbol);



// These are not used for debug info generation, but rather as general function
// common to both lc3 targets.

int calculate_address(int dstReg, int baseReg_, int offset, int withLDR, FILE *output);

#endif
