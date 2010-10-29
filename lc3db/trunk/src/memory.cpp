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
  mem = new int16_t[0x10000];
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
  for (i = PC; i < (PC + stats.st_size - 2); i++) {
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

int Memory::add_source_file(std::string filePath) 
{
  int i;
  for (i=0; i < filePaths.size(); i++) {
    if (filePath == filePaths[i]) {
      return i;
    }
  }

  addresses.push_back(std::vector<uint16_t>());

  size_t pos = filePath.length()-1; 
  while(pos >= 0 && filePath[pos] != '/') {
    pos--;
  }
  fileNames.push_back(filePath.substr(pos+1));

  filePaths.push_back(filePath);
  return filePaths.size()-1;
}

void Memory::add_source_line(uint16_t address, int fileId, int lineNo) 
{
  assert(fileId < filePaths.size());
  // Add element for source lookup
  sources[address] = SourceLocationInt(fileId, lineNo);
  std::vector<uint16_t> &pfa = addresses[fileId];

  // Add element for address lookup
  int lastLine = pfa.size()-1;
  int lastAddress = lastLine < 0 ? address : pfa[lastLine];
  for (int i=lastLine+1; i < lineNo; i++) {
    pfa.push_back(lastAddress);
  }
  pfa.push_back(address);
}

uint16_t Memory::find_address(std::string fileName, int lineNo) 
{
  int i;
  for (i=0; i < filePaths.size(); i++) {
    if (fileName == fileNames[i]) {
      if (addresses[i].size() > lineNo) {
        return addresses[i][lineNo];
      } else {
	return 0;
      }
    }
  }

  return 0;
}

SourceLocationInt Memory::find_source_location(uint16_t address) 
{
  std::map<uint16_t, SourceLocationInt>::const_iterator it;

  it = sources.find(address);
  if (it != sources.end()) {
    return it->second;
  }

  return SourceLocationInt();
}

SourceLocation Memory::find_source_location_short(uint16_t address) 
{
  SourceLocationInt sl = find_source_location(address);
  return SourceLocation(fileNames[sl.fileId].c_str(), sl.lineNo);
}

SourceLocation Memory::find_source_location_absolute(uint16_t address) 
{
  SourceLocationInt sl = find_source_location(address);
  return SourceLocation(filePaths[sl.fileId].c_str(), sl.lineNo);
}


// vim: sw=2 si:
