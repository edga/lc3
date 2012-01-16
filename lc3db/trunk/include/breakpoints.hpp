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

#ifndef _BREAKPOINTS_HPP
#define _BREAKPOINTS_HPP

#include <string>
#include <list>
#include <set>
#include <stdint.h>

#include "source_info.hpp"

enum BreakpointDisposition
{
  Keep,
  Disable,
  Delete
};

enum BreakpointKind
{
  bkBreakpoint,
  bkWatchpoint,
  bkRwatchpoint,
  bkAwatchpoint
};

struct Breakpoint;

typedef std::list<Breakpoint*>::iterator BreakpointIterator;

class UserBreakpoits {
public:
  UserBreakpoits(SourceInfo &_src_info) :
    src_info(_src_info), last_id(0) {};
  // ~UserBreakpoits();
/*
  Usage:
  * Create new Breakpoint (by address/by source location/by label)
  * Delete breakpoint
  * Enable/Disable Breakpoint
  * Set ignore count	(on hit action)
  * Enable once/delete  (on hit action)
  * Hit: Check for active breakpoint at address
 */
  int addWatch(uint16_t address, bool temp, BreakpointKind kind);
  int add(uint16_t address, bool temp);
  int add(uint16_t address, std::string fileName, int lineNo);
  int erase(int id);
  int setEnabled(int id, bool enable,bool setDisp, BreakpointDisposition disp);
  int setIgnoreCount(int id, int count);
  int setOnHit(int id, BreakpointDisposition disp);
  int check(uint16_t address);
  void showInfo();

private:
  SourceInfo &src_info;
  int last_id; 
  // watchpoints
  std::set<uint16_t> w_watchpoints;
  std::set<uint16_t> r_watchpoints;
  // Active breakpoints for quick check before each execution cycle
  // Note that we don't support multiple breakpoints for same location (even though gdb does it).
  // Multiple breakpoints at same location don't make much sense without conditional breakpoints (which are not supported here).
  std::set<uint16_t> active_breakpoints;
  std::list<Breakpoint*> breakpoints; // Full information about user breakpoints
  BreakpointIterator lookupA(uint16_t address);
  BreakpointIterator lookupI(int id);
  int erase(BreakpointIterator it);
  int setEnabled(BreakpointIterator it, bool enable, bool setDisp, BreakpointDisposition disp);
  int setIgnoreCount(BreakpointIterator it, int count);
};


#endif
