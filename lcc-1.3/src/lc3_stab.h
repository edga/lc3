#ifndef _LC3_STAB_H
#define _LC3_STAB_H

#include "c.h"

extern int lc3_stabFileId;

void lc3_stabblock(int brace, int lev, Symbol *p);
void lc3_stabinit(char *, int, char *[]);
void lc3_stabline(Coordinate *);
void lc3_stabsym(Symbol);

#endif
