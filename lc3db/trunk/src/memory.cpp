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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include "memory.hpp"

MappedWord::~MappedWord() { }

Word::Word(Memory &mem, uint16_t address)
  : value(mem.mem[address]), mapped(mem.mapped_word(address))
{
}

Word::operator int16_t() const
{
  if (mapped) {
    return *mapped;
  }

  return value;
}

Word &Word::operator=(int16_t rhs)
{
  if (mapped) {
    *mapped = rhs;
  } else {
    value = rhs;
  }

  return *this;
}

Word Memory::operator[](uint16_t index)
{
  return Word(*this, index);
}

MappedWord *Memory::mapped_word(uint16_t index)
{
  if (dma.count(index)) {
    return dma[index];
  }
  return 0;
}

Memory::Memory()
{
  const int size = 0x10000;
  mem = new int16_t[size];
#warning "valgrind doesn't like memset/bzero"
  //bzero(&mem[0], (&mem[size]-&mem[0]));
  for (int i=0; i < size; i++) {
    mem[i] = 0;
  }
}

Memory::~Memory()
{
  delete [] mem;
}

uint16_t Memory::load(const std::string &filename)
{
  int fd = open(filename.c_str(), O_RDONLY);
  struct stat stats;
  uint16_t i;
  uint16_t PC;

  if (fd == -1) {
    return 0xFFFF;
  }
  if (fstat(fd, &stats) == -1) {
    perror("fstat");
    return 0xFFFF;
  }
  ::read(fd, &PC, 2);
#if __BYTE_ORDER == __LITTLE_ENDIAN
  PC = ((PC << 8) | ((PC >> 8) & 0x00FF));
#endif
  ::read(fd, &mem[PC], stats.st_size - 2);
#if __BYTE_ORDER == __LITTLE_ENDIAN
  for (i = PC; i < (PC + stats.st_size/2 - 1); i++) {
    mem[i] = ((mem[i] << 8) | ((mem[i] >> 8) & 0x00FF));
  }
#endif
  close(fd);
  
  return PC;
}
  
void Memory::cycle()
{
  for (dma_map_t::iterator i = dma.begin(); i != dma.end(); ++i) {
    i->second->cycle();
  }
}

void Memory::register_dma(uint16_t address, MappedWord *word)
{
  dma[address] = word;
}

// vim: sw=2 si:
