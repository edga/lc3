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

// Internal source location
struct _SourceLocation
{
  int fileId;
  int lineNo;

  _SourceLocation() :
    fileId(0),lineNo(-1) {}
  _SourceLocation(const int _fileId,const int _lineNo) :
    fileId(_fileId),lineNo(_lineNo) {}
};	

SourceInfo::SourceInfo() {}
SourceInfo::~SourceInfo() {}

int SourceInfo::add_source_file(std::string filePath) 
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

void SourceInfo::add_source_line(uint16_t address, int fileId, int lineNo) 
{
  assert(fileId < filePaths.size());
  // Add element for source lookup
  sources[address] = _SourceLocation(fileId, lineNo);
  std::vector<uint16_t> &pfa = addresses[fileId];

  // Add element for address lookup
  int lastLine = pfa.size()-1;
  int lastAddress = lastLine < 0 ? address : pfa[lastLine];
  for (int i=lastLine+1; i < lineNo; i++) {
    pfa.push_back(lastAddress);
  }
  pfa.push_back(address);
}

uint16_t SourceInfo::find_address(std::string fileName, int lineNo) 
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

_SourceLocation SourceInfo::find_source_location(uint16_t address) 
{
  std::map<uint16_t, _SourceLocation>::const_iterator it;

  it = sources.find(address);
  if (it != sources.end()) {
    return it->second;
  }

  return _SourceLocation();
}

SourceLocation SourceInfo::find_source_location_short(uint16_t address) 
{
  _SourceLocation sl = find_source_location(address);
  return SourceLocation(fileNames[sl.fileId].c_str(), sl.lineNo);
}

SourceLocation SourceInfo::find_source_location_absolute(uint16_t address) 
{
  _SourceLocation sl = find_source_location(address);
  return SourceLocation(filePaths[sl.fileId].c_str(), sl.lineNo);
}


// vim: sw=2 si:
