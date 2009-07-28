/*\
 *  LC-3 Simulator
 *  Copyright (C) 2004  Anthony Liguori <aliguori@cs.utexas.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
\*/

#ifndef _UTIL_HPP
#define _UTIL_HPP

#define A -1
#define B -2
#define C -3
#define D -4 
#define E -5
#define F -6
#define G -7

#define _BITS(cond, action, I, a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p) \
 ((cond (I,p,a,b) ? action (15) : 0) \
| (cond (I,a,b,c) ? action (14) : 0) \
| (cond (I,b,c,d) ? action (13) : 0) \
| (cond (I,c,d,e) ? action (12) : 0) \
| (cond (I,d,e,f) ? action (11) : 0) \
| (cond (I,e,f,g) ? action (10) : 0) \
| (cond (I,f,g,h) ? action ( 9) : 0) \
| (cond (I,g,h,i) ? action ( 8) : 0) \
| (cond (I,h,i,j) ? action ( 7) : 0) \
| (cond (I,i,j,k) ? action ( 6) : 0) \
| (cond (I,j,k,l) ? action ( 5) : 0) \
| (cond (I,k,l,m) ? action ( 4) : 0) \
| (cond (I,l,m,n) ? action ( 3) : 0) \
| (cond (I,m,n,o) ? action ( 2) : 0) \
| (cond (I,n,o,p) ? action ( 1) : 0) \
| (cond (I,o,p,a) ? action ( 0) : 0))

#define STRIP_PAREN(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p) \
  a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p
#define CALL(a, b, c, d, e) a (b, c, d, e)
#define BITS(cond, a, action, b) CALL(_BITS, cond, action, a, STRIP_PAREN b)

#define IS_BIT(I,a,b,c) (b == 1 || b == 0)
#define EQUALS(I,a,b,c) (b == I)
#define FIRST(I,a,b,c) (b == I && a != I)
#define LAST(I,a,b,c) (b == I && c != I)

#define RSHIFT(a) (1 << a)
#define NOP(a) (a)

#define MASK(a, bits) BITS(EQUALS, a, RSHIFT, bits)
#define ZEXT(a, b, bits) ((b & MASK(a, bits)) >> BITS(LAST, a, NOP, bits))
#define SEXT(a, b, bits) (ZEXT(a, b, bits) | ((BITS(FIRST, a, RSHIFT, bits) & b) ? ((0xFFFF << BITS(FIRST, a, NOP, bits)) & 0xFFFF) : 0))

#define SHIFT(a, bits) BITS(LAST, a, NOP, bits)

#define OP_EQ(inst, bits) \
((inst & BITS(IS_BIT, 0, RSHIFT, bits)) == BITS(EQUALS, 1, RSHIFT, bits))

#endif
