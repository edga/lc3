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

#ifndef _CPU_HPP
#define _CPU_HPP

#include <string>
#include "memory.hpp"

namespace LC3 {

class CPU
{
public:
  CPU(Memory &mem);
  void cycle();
  void decode(uint16_t IR);
  void interrupt(uint16_t signal, uint16_t priority);

  uint16_t PC;
  uint16_t PSR;
  int16_t R[8];
  uint16_t USP;
  uint16_t SSP;

private:
  Memory &mem;
};

}

#endif
