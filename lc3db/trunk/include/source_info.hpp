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
#include <map>
#include <stdint.h>

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

class SourceInfo {
public:
  SourceInfo();
  ~SourceInfo();

  int add_source_file(int fileId, std::string filePath);
  void add_source_line(uint16_t firstAddress, uint16_t lastAddress, int fileId, int lineNo);
  SourceLocation find_source_location_short(uint16_t address); 
  SourceLocation find_source_location_absolute(uint16_t address); 
  uint16_t find_line_start_address(std::string fileName, int lineNo); 
  std::map<std::string, uint16_t> symbol;

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
