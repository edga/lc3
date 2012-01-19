/*\
 *  This file is part of LC-3 Simulator.
 *  Copyright (C) 2010  Edgar Lakis <edgar.lakis@gmail.com>
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

#ifndef _SOURCE_INFO_HPP
#define _SOURCE_INFO_HPP

#include <string>
#include <vector>
#include <list>
#include <map>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

// External source location representation
struct SourceLocation
{
  const char * fileName;
  int lineNo;
  bool isHLLSource;
  uint16_t firstAddr, lastAddr;

  SourceLocation(const char *_fileName, int _lineNo, bool _isHLLSource, uint16_t _firstAddr, uint16_t _lastAddr) :
    fileName(_fileName),lineNo(_lineNo),
    isHLLSource(_isHLLSource), firstAddr(_firstAddr), lastAddr(_lastAddr) {}
};	

// Internal source location representation
struct _SourceLocation;
struct _HLLSourceLocation;
struct _MachineSourceLocation;

enum VariableKind {
	FileGlobal,
	FileStatic,
	FunctionStatic,
	FunctionLocal,
	FunctionParameter,
	AssemblerLabel,
	CpuSpecial
};

struct VariableInfo{
  const char* name;
  VariableKind kind;
  char modificator;	// '&':address, '*':dereference, ' ':none 
  int typeId;
  bool isCpuSpecial;
  bool isAddressAbsolute;
  int address;	// LC3 absolute address if isAddressAbsolute; else frame (R5) offset
  VariableInfo(const char*_name, VariableKind _kind) :
    name(strdup(_name)), kind(_kind), modificator(' '),
    typeId(0), isCpuSpecial(1), isAddressAbsolute(0), address(0) 
  {
    assert(kind==CpuSpecial);
  }
  VariableInfo(const char*_name, char _modificator, VariableKind _kind, int _typeId, int _address) :
    name(strdup(_name)), kind(_kind), modificator(_modificator), typeId(_typeId), address(_address) 
  {
    isCpuSpecial = _kind==CpuSpecial;
    isAddressAbsolute = _kind==FileGlobal || _kind==FileStatic || _kind==FunctionStatic || _kind==AssemblerLabel;
  }
  VariableInfo(const char*_name, VariableKind _kind, int _typeId, int _address) {
	  VariableInfo(_name, ' ', _kind, _typeId, _address);
  }
  
//  // Empty VariableInfo
//  VariableInfo() {
//    VariableInfo(NULL, CpuSpecial);
//  }
//  // Empty VariableInfo
//  operator bool() const {
//    name && kind != CpuSpecial;
//  }
//
//
//  ~VariableInfo() {
//    free((void*)name);
//  }
//
//  VariableInfo(const VariableInfo& old_vi) {
//    name = strdup(old_vi.name);
//  }
//
//  VariableInfo& operator=( const VariableInfo &rhs ) {
//    if( this == &rhs )
//    {
//	  return *this;
//    }  
//
//    free((void*)name);
//    name = strdup(rhs.name);
//
//    return *this;
//  }
};

struct SourceBlock;
struct FunctionInfo{
  const char* name;
  bool isStatic;
  int returnTypeId;
  uint16_t entry;
  SourceBlock *block;
  std::list<VariableInfo*> args;
  FunctionInfo(bool _isStatic, int _returnTypeId, const char *_name, uint16_t _entry) :
    name(strdup(_name)), isStatic(_isStatic), returnTypeId(_returnTypeId), entry(_entry),
    block(NULL) {}
}; 

struct SourceBlock{
  FunctionInfo *function;
  int level;
  SourceBlock *parent;
  uint16_t start;
  uint16_t end;
  std::list<VariableInfo*> variables;
  SourceBlock(int _level, SourceBlock *_parent, uint16_t _start, FunctionInfo *_function) :
    level(_level), start(_start), parent(_parent), function(_function),
	end(0) {}
};

class SourceInfo {
public:
  SourceInfo();
  ~SourceInfo();

  void reset_HLL_info(void);
  int add_source_file(int fileId, std::string filePath);
  void add_source_line(uint16_t firstAddress, uint16_t lastAddress, int fileId, int lineNo);
  void add_type(int typeId, const char* typeDescriptor);
  void start_declaration_block(const char* functionName, int level, uint16_t addresses);
  void finish_declaration_block(const char* functionName, int level, uint16_t addresses);
  void add_absolute_variable(VariableKind kind, int typeId, const char* sourceName, const char* assemblerLabel);
  void add_stack_variable(VariableKind kind, int typeId, const char* sourceName, int frameOffset);
  void add_function(bool isStatic, int returnTypeId, const char* sourceName, const char* assemblerLabel);

  SourceLocation find_source_location_short(uint16_t address);
  SourceLocation find_source_location_absolute(uint16_t address);
  uint16_t find_line_start_address(std::string fileName, int lineNo);
  std::map<std::string, uint16_t> symbol;
  
  // Todo: FixMe: Handle FileStatic variables per each source file
  std::map<std::string, VariableInfo*> globalVariables;
  std::map<uint16_t, SourceBlock*> sourceBlocks;
  SourceBlock* find_source_block(uint16_t address);
  VariableInfo* find_variable(uint16_t scopeAddress, const char* name);

private:
  _SourceLocation find_source_location(uint16_t address); 
  std::map<uint16_t, _MachineSourceLocation> machineSources; // lc3 address => source file location
  std::map<uint16_t, _HLLSourceLocation> HLLSources;         // lc3 address => source file location
  std::map<uint16_t, uint16_t> internalIds;    // userFileId => internalFileId
  std::vector<std::string> fileNames;	// internalFileID => short file name
  std::vector<std::string> filePaths;	// internalFileID => full path
  std::vector<std::vector<uint16_t> > startAddresses; // source file location => lc3 address (addresses[fileId][lineNo])
};


#endif
