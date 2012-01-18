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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stack>
#include <algorithm>
#include "source_info.hpp"

// Internal source locations
struct _MachineSourceLocation
{
  int fileId;
  int lineNo;

  _MachineSourceLocation() :
    fileId(0),lineNo(-1) {}
  _MachineSourceLocation(const int _fileId,const int _lineNo) :
    fileId(_fileId),lineNo(_lineNo) {}

};

struct _HLLSourceLocation
{
  int fileId;
  int lineNo;
  uint16_t firstAddr, lastAddr;

  _HLLSourceLocation() :
    fileId(0),lineNo(-1),
    firstAddr(0), lastAddr(0) {}
  _HLLSourceLocation(const int _fileId,const int _lineNo, uint16_t _firstAddr, uint16_t _lastAddr) :
    fileId(_fileId),lineNo(_lineNo),
    firstAddr(_firstAddr), lastAddr(_lastAddr) {}
};	

struct _SourceLocation
{
  int fileId;
  int lineNo;
  bool isHLLSource;
  uint16_t firstAddr, lastAddr;

  _SourceLocation() :
    fileId(0),lineNo(-1),
    isHLLSource(false), firstAddr(0), lastAddr(0) {}
  _SourceLocation(int _fileId, int _lineNo, bool _isHLLSource, uint16_t _firstAddr, uint16_t _lastAddr) :
    fileId(_fileId),lineNo(_lineNo),
    isHLLSource(_isHLLSource), firstAddr(_firstAddr), lastAddr(_lastAddr) {}

  _SourceLocation(_MachineSourceLocation ml, uint16_t addr) :
    fileId(ml.fileId),lineNo(ml.lineNo),
    isHLLSource(false), firstAddr(addr), lastAddr(addr) {}
  _SourceLocation(_HLLSourceLocation ml) :
    fileId(ml.fileId),lineNo(ml.lineNo),
    isHLLSource(true), firstAddr(ml.firstAddr), lastAddr(ml.lastAddr) {}

  SourceLocation toUser(std::vector<std::string> names) {
    return SourceLocation(names[fileId].c_str(), lineNo, isHLLSource, firstAddr, lastAddr);
  }
};


SourceInfo::SourceInfo() {}
SourceInfo::~SourceInfo() {}

void SourceInfo::reset_HLL_info(void)
{
  // FixMe: Is this enough or should we clean-up each element (find out about memory management of the map). 
  HLLSources.clear();
}

int SourceInfo::add_source_file(int fileId, std::string filePath)
{
  int i;
  for (i=0; i < filePaths.size(); i++) {
    if (filePath == filePaths[i]) {
      internalIds[fileId] = i;
      return i;
    }
  }

  startAddresses.push_back(std::vector<uint16_t>());

  int pos = filePath.length()-1;
  while(pos >= 0 && filePath[pos] != '/' && filePath[pos] != '\\') {
    pos--;
  }
  fileNames.push_back(filePath.substr(pos+1));

  filePaths.push_back(filePath);

  i = filePaths.size()-1;
  internalIds[fileId] = i;
  return i;
}

/* Adds information about source line:
 * it is machine source line if userFileId == 0
 */
void SourceInfo::add_source_line(uint16_t lineStart, uint16_t lineEnd, int userFileId, int lineNo)
{
  int internalFileId = internalIds[userFileId];
  assert(internalFileId < filePaths.size());

  // Add element for source lookup
  if (userFileId == 0) { // Machine source
    assert(lineStart == lineEnd);
    machineSources[lineStart] = _MachineSourceLocation(internalFileId, lineNo);
  } else { // High Level Language source
    _HLLSourceLocation h = _HLLSourceLocation(internalFileId, lineNo, lineStart, lineEnd);
    //fprintf(stderr, "+%.4x: [%.4x,%.4x) in %d:%d\n", lineStart, h.firstAddr, h.lastAddr, h.fileId, h.lineNo);
    HLLSources[lineStart] = h;
  }

  // Add element for address lookup
  std::vector<uint16_t> &pfa = startAddresses[internalFileId];
  int prevLine = pfa.size()-1;
  for (int i=prevLine+1; i < lineNo; i++) {
    pfa.push_back(lineStart);
  }
  pfa.push_back(lineStart);
}

/* Add type definition to the debugger:
 * - typeDescriptor is string representation of the type.
 *   It uses character codes to describe kind of composite types (struct/array/function...) and typeId's of previously added types.
 * - typeId is code by which the given type will be further referenced (by variables or other types)
 */
void SourceInfo::add_type(int typeId, const char* typeDescriptor){
  // FixMe: need to deal with forward declarations (code 'x') of structs/...
 
  // All types are hardcoded to int for now, this function doesn't do anything
}

static FunctionInfo *currentFunction = NULL;
static SourceBlock *currentBlock = NULL;
#define currentBlockLevel (currentBlock ? currentBlock->level : -1)
static std::stack<SourceBlock*> openedBlocks;
/* Start a variable declaration block (a new local scope for variables, left brace symbol '{' in C)
 * - functionName: name of C function to which this block belongs
 * - level: the level for nested scopes (0 is the block for the whole function)
 * - address: the machine address where the block starts
 */
void SourceInfo::start_declaration_block(const char* functionName, int level, uint16_t address){
  // All types are hardcoded to int for now, this function doesn't do anything
  fprintf(stderr, "%*s{  // in %s at 0x%04x\n", level*8, "", functionName, address);
  assert( (!currentBlock && !level) ||
     	 (level==(currentBlock->level+1)) );
  currentBlock = new SourceBlock(level, currentBlock, address, currentFunction);
  openedBlocks.push(currentBlock);
  if (level == 0) {
    assert(currentFunction);
    currentFunction->block = currentBlock;
  }
}

/* Finish a variable declaration block (last created local scope for variables, right brace symbol '}' in C)
 * - functionName: name of C function to which this block belongs
 * - level: the level for nested scopes (0 is the block for the whole function)
 * - address: the machine address where the block ends
 */
void SourceInfo::finish_declaration_block(const char* functionName, int level, uint16_t address){
  fprintf(stderr, "%*s}  // in %s at 0x%04x\n", level*8, "", functionName, address);
  assert( currentBlock && (level==(currentBlock->level)) );
  if (level != 0 && currentBlock->variables.empty()) {
    delete currentBlock;
  } else {
    currentBlock->end = address;
    sourceBlocks[currentBlock->start] = currentBlock;
  }
  openedBlocks.pop();
  currentBlock = openedBlocks.empty() ? NULL : openedBlocks.top();
}

/* Add information about the absolute variable (not stack frame relative):
 * - kind: the kind of variable, to distinguish between FileGlobal, FileStatic and FunctionStatic   
 * - typeId: the variable type (must be previously registered by calling add_type())
 * - sourceName: the name of variable in the C source (as seen by the user)
 * - assemblerLabel: the assembler label which points the variable start address
 */
void SourceInfo::add_absolute_variable(VariableKind kind, int typeId, const char* sourceName, const char* assemblerLabel){
  if (kind==FileGlobal) {
	fprintf(stderr, "T%d %s \t// at %s\n", typeId, sourceName, assemblerLabel);
  } else if (kind==FileStatic) {
	fprintf(stderr, "static T%d %s \t// at %s\n", typeId, sourceName, assemblerLabel);
  } else if (kind==FunctionStatic) {
	fprintf(stderr, "%*sstatic T%d %s \t// at %s\n", currentBlockLevel*8+8, "", typeId, sourceName, assemblerLabel);
  }

  assert(!(currentBlock && (kind==FileGlobal || kind==FileStatic)));
  assert(!(!currentBlock && kind==FunctionStatic));
  assert(symbol.count(assemblerLabel)==1);

  VariableInfo *v = new VariableInfo(sourceName, kind, typeId, symbol[assemblerLabel]);

  if (kind==FunctionStatic) {
    currentBlock->variables.push_back(v);
  } else {
    // Todo: FixMe: Handle FileStatic variables per each source file
    globalVariables[v->name] = v;
  }
}

/* Add information about the stack frame relative variable:
 * - kind: the kind of variable, to distinguish between FunctionLocal and FunctionParameter
 * - typeId: the variable type (must be previously registered by calling add_type())
 * - sourceName: the name of variable in the C source (as seen by the user)
 * - frameOffset: the offset from current frame (the frame is pointed by R5,
 *   the positive offsets are used for parameters and the rest for locals)
 */
void SourceInfo::add_stack_variable(VariableKind kind, int typeId, const char* sourceName, int frameOffset){
  if (kind==FunctionParameter) {
	fprintf(stderr, "     param T%d %s \t// at R5[%d]\n", typeId, sourceName, frameOffset);
  } else if (kind==FunctionLocal) {
	fprintf(stderr, "%*sT%d %s \t// at R5[%d]\n", currentBlockLevel*8+8, "", typeId, sourceName, frameOffset);
  }

  VariableInfo *v = new VariableInfo(sourceName, kind, typeId, frameOffset);
  if (kind==FunctionParameter) {
    assert(currentFunction);
    currentFunction->args.push_back(v);
  } else {
    assert(currentBlock);
    currentBlock->variables.push_back(v);
  }
}

/* Add information about the function:
 * - isStatic: the kind of the function, to distinguish between static and global
 * - returnTypeId: the type of return value (must be previously registered by calling add_type())
 * - sourceName: the name of function in the C source (as seen by the user)
 * - assemblerLabel: the assembler label which points the function start address (entry point)
 */
void SourceInfo::add_function(bool isStatic, int returnTypeId, const char* sourceName, const char* assemblerLabel){
  fprintf(stderr, "%s%s returns T%d \t// at %s\n", isStatic?"static ":"", sourceName, returnTypeId, assemblerLabel);

  assert(symbol.count(assemblerLabel)==1);
  currentFunction = new FunctionInfo(isStatic, returnTypeId, sourceName, symbol[assemblerLabel]);
}

SourceBlock* SourceInfo::find_source_block(uint16_t address){
  std::map<uint16_t, SourceBlock*>::const_iterator it;

  it = sourceBlocks.upper_bound(address);
  if (it != sourceBlocks.begin()) {
    it--;	// Find the largest block where block.start <= address
    SourceBlock *sb = it->second;
    while(sb) {
      if (
	address >= sb->start &&
	address <= sb->end) {
	return sb;
      }
      sb = sb->parent;
    }
  }

  return NULL;
}


VariableInfo* SourceInfo::find_variable(uint16_t scopeAddress, const char* name){
  SourceBlock* scope = find_source_block(scopeAddress);
  if (scope) {
    std::list<VariableInfo*>::const_iterator it;
    FunctionInfo* function = scope->function;
    
    // Try Locals, than arguments, than globals
    while(scope) {
      for (it=scope->variables.begin(); it != scope->variables.end(); it++) {
	if (strcmp((*it)->name, name)==0){
	  return *it;
	}
      }
      scope = scope->parent;
    }
    
    // Try arguments: 
    for (it=function->args.begin(); it != function->args.end(); it++) {
      if (strcmp((*it)->name, name)==0){
	return *it;
      }
    }
    
    // Try globals: 
    if (globalVariables.count(name)) {
      return globalVariables[name];
    }
  }

  return NULL;
}

uint16_t SourceInfo::find_line_start_address(std::string fileName, int lineNo) 
{
  int i;
  for (i=0; i < filePaths.size(); i++) {
    if (fileName == fileNames[i]) {
      if (startAddresses[i].size() > lineNo) {
        return startAddresses[i][lineNo];
      } else {
	return 0;
      }
    }
  }

  return 0;
}

_SourceLocation SourceInfo::find_source_location(uint16_t address) 
{
  std::map<uint16_t, _HLLSourceLocation>::const_iterator hit;
  std::map<uint16_t, _MachineSourceLocation>::const_iterator mit;

  /* Try higher level language info
   */
  hit = HLLSources.upper_bound(address);
  if (hit != HLLSources.begin()) {
    hit--;
    //_HLLSourceLocation h = hit->second;
    //fprintf(stderr, "LOW: %.4x: [%.4x,%.4x) in %d:%d\n", hit->first, h.firstAddr, h.lastAddr, h.fileId, h.lineNo);
    if (
      address >= hit->second.firstAddr &&
      address <= hit->second.lastAddr) {
      return _SourceLocation(hit->second);
    }
  }

  /* Fall back to assembly info
   */
  mit = machineSources.find(address);
  if (mit != machineSources.end()) {
    return _SourceLocation(mit->second, address);
  }

  /* No info
   */
  return _SourceLocation();
}

SourceLocation SourceInfo::find_source_location_short(uint16_t address) 
{
  _SourceLocation sl = find_source_location(address);
  return sl.toUser(fileNames);
}

SourceLocation SourceInfo::find_source_location_absolute(uint16_t address) 
{
  _SourceLocation sl = find_source_location(address);
  return sl.toUser(filePaths);
}


// vim: sw=2 si:
