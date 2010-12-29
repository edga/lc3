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
  while(pos >= 0 && filePath[pos] != '/') {
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
