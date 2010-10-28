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

struct SourceLocation
{
  int fileId;
  int lineNo;

  SourceLocation() :
    fileId(0),lineNo(-1) {}
  SourceLocation(const int _fileId,const int _lineNo) :
    fileId(_fileId),lineNo(_lineNo) {}
};	

class Memory {
public:
  Memory();
  ~Memory();

  Word operator[](uint16_t index);
  uint16_t load(const std::string &filename);
  void cycle();
  void register_dma(uint16_t address, MappedWord *word);
  int add_source_file(std::string filePath); 
  void add_source_line(uint16_t address, int fileId, int lineNo); 
  const char * find_source_path(uint16_t address); 
  uint16_t find_address(std::string fileName, int lineNo); 
  std::map<uint16_t, SourceLocation> sources; // lc3 address => source file location
  std::vector<std::string> fileNames;	// fileID => short file name
  std::vector<std::string> filePaths;	// fileID => full path
  std::vector<std::vector<uint16_t> > addresses; // source file location => lc3 address (addresses[fileId][lineNo])
  std::map<std::string, uint16_t> symbol;

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
