/*\
 *  LC-3 Simulator
 *  Copyright (C) 2004  Anthony Liguori <aliguori@cs.utexas.edu>
 *  Modifications 2010  Edgar Lakis <edgar.lakis@gmail.com>
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

#ifndef _MEMORY_HPP
#define _MEMORY_HPP

#include <string>
#include <vector>
#include <map>
#include <stdint.h>

class MappedWord;
class Memory;

struct Word
{
  Word(Memory &mem, uint16_t address);

  operator int16_t() const;
  Word &operator=(int16_t rhs);

private:
  int16_t &value;
  MappedWord *mapped;
};

class Memory {
public:
  Memory();
  ~Memory();

  Word operator[](uint16_t index);
  uint16_t load(const std::string &filename);
  void cycle();
  void register_dma(uint16_t address, MappedWord *word);

private:
  MappedWord *mapped_word(uint16_t index);
  typedef std::map<uint16_t, MappedWord *> dma_map_t;
  dma_map_t dma;
  friend class Word;

  int16_t *mem;
};

struct MappedWord
{
  virtual ~MappedWord() = 0;
  virtual operator int16_t() const { return 0; };
  virtual MappedWord &operator=(int16_t rhs) { return *this; };
  virtual void cycle() { };
};

#endif
