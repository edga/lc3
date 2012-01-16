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

#include <stdio.h>
#include <sstream>
#include "breakpoints.hpp"

struct Breakpoint
{
  int id;
  BreakpointKind kind;
  BreakpointDisposition disposition;
  bool enabled;
  uint16_t address;
  int ignore_count;
  int hits;
  std::string file;
  int line;

  Breakpoint(int _id, uint16_t _address, std::string _file, int _line) :
    id(_id), address(_address), file(_file), line(_line),
    kind(bkBreakpoint),
    disposition(Keep), enabled(true), ignore_count(0), hits(0) {}
};	

//////////////////////////////////////////////////////////
// User interface messages related to breakpoint commands
class BreakpointsUI {
public:
  // New:
  //   Breakpoint 8 at 0x80483ed: file test_struct.c, line 10.
  //   Temporary breakpoint 8 at 0x80483ed: file test_struct.c, line 10.
  static void creation(int id, uint16_t address, bool temporary, const char *file, int line) {
    if (line > 0) {
      printf("%s %d at 0x%04x: file %s, line %d.\n",
	     temporary ? "Temporary breakpoint" : "Breakpoint",
	     id, address, file, line);
    } else {
      printf("%s %d at 0x%04x.\n",
	     temporary ? "Temporary breakpoint" : "Breakpoint",
	     id, address );
    }
  }

  // Hit:
  //   "Breakpoint 7, 0x08048406 in main (argc=1, argv=0xbffff414) at test_struct.c:27"
  //   "Temporary breakpoint 7, 0x08048406 in main (argc=1, argv=0xbffff414) at test_struct.c:27"
  static void hit(int id, uint16_t address, bool temporary, const char *file, int line) {
    if (line > 0) {
      printf("%s %d, 0x%04x: at %s:%d.\n",
	     temporary ? "Temporary breakpoint" : "Breakpoint",
	     id, address, file, line);
    } else {
      printf("%s %d, 0x%04x.\n",
	     temporary ? "Temporary breakpoint" : "Breakpoint",
	     id, address);
    }
  }

  // List:
  //	Num     Type           Disp Enb Address    What
  //	1       breakpoint     dis  n   0x0804841a in main at test_struct.c:29
  //	 	breakpoint already hit 2 times
  //	 	ignore next 2 hits
  //	2       breakpoint     dis  n   0x080483f7 in main at test_struct.c:27
  static void list_header(bool not_empty){
    if (not_empty) {
      printf("%-8s%-15s%-5s%-4s%-11s%s\n",
	  "Num", "Type", "Disp", "Enb", "Address", "What");
    } else {
      printf("No breakpoints or watchpoints.\n");
    }
  }
  static void list_line(int id, uint16_t address, BreakpointDisposition disp, bool enabled, int hits, int ignore, const char *file, int line){
    const char * dispStr = "keep";
    if (disp == Delete) {
      dispStr = "del";
    } else if (disp == Disable) {
      dispStr = "dis";
    }
    
    if (line > 0) {
      printf("%-8d%-15s%-5s%-4s0x%-9.4xat %s:%d\n",
	     //   "Num", "Type", "Disp", "Enb", "Address", "What",
	     id, "breakpoint", dispStr, enabled ? "y" : "n", address, file, line);
    } else {
      printf("%-8d%-15s%-5s%-4s0x%-9.4x<no_location>\n",
	     //   "Num", "Type", "Disp", "Enb", "Address", "What",
	     id, "breakpoint", dispStr, enabled ? "y" : "n", address);
    }
    if (hits) printf("\t	breakpoint already hit %d times\n", hits);
    if (ignore) printf("\t	ignore next %d hits\n", ignore);
  }
  static void list_line(Breakpoint *b){
    list_line(b->id, b->address, b->disposition, b->enabled, b->hits, b->ignore_count, b->file.c_str(), b->line);
  }

  // Set Ignore Count
  //   printf("Will ignore next %d crossings of breakpoint %d.\n", count, res);
  static void ignore_success(int id, int count){
    printf("Will ignore next %d crossings of breakpoint %d.\n", count, id);
  }
  // Wrong breakpoint id
  static void no_breakpoint(int id){
    printf("No breakpoint number %d.\n", id);
  }
  // Duplicates not allowed
  static void duplicate(uint16_t address, int id){
    printf("Preakpoint for address 0x%04x already defined, see breakpoint %d.\n", address, id);
  }
};


//////////////////////////////////////////////////////////
// UserBreakpoits class
int UserBreakpoits::addWatch(uint16_t address, bool temp, BreakpointKind kind)
{ 
  /*   Might avoid duplicate later 
  BreakpointIterator it = lookupA(address);
  if (it != breakpoints.end()) {
    BreakpointsUI::duplicate(address, (*it)->id);
    return -1;
  }
  */

  Breakpoint* b = new Breakpoint(++last_id, address, "", -1);
  if (temp) {
    b->disposition = Delete;
  }
  b->kind = kind;
  if (kind == bkWatchpoint || kind == bkAwatchpoint) {
    w_watchpoints.insert(address);
  }
  if (kind == bkRwatchpoint || kind == bkAwatchpoint) {
    r_watchpoints.insert(address);
  }

  breakpoints.push_back(b);

  //BreakpointsUI::creation(last_id, address, temp, sl.fileName, sl.lineNo);

  return last_id;
}

int UserBreakpoits::add(uint16_t address, bool temp)
{ 
  BreakpointIterator it = lookupA(address);
  if (it != breakpoints.end()) {
    BreakpointsUI::duplicate(address, (*it)->id);
    return -1;
  }

  SourceLocation sl = src_info.find_source_location_short(address);

  Breakpoint* b = new Breakpoint(++last_id, address, sl.fileName, sl.lineNo);
  if (temp) {
    b->disposition = Delete;
  }
  breakpoints.push_back(b);
  active_breakpoints.insert(address);
  //BreakpointsUI::creation(last_id, address, temp, sl.fileName, sl.lineNo);

  return last_id;
}


int UserBreakpoits::erase(BreakpointIterator it)
{
  active_breakpoints.erase((*it)->address);
  breakpoints.erase(it);
  int id = (*it)->id;
  delete *it;

  return id;
}

int UserBreakpoits::erase(int id)
{
  BreakpointIterator it = lookupI(id);
  if (it == breakpoints.end()) {
    BreakpointsUI::no_breakpoint(id);
    return -1;
  }

  return erase(it);
}

int UserBreakpoits::setEnabled(BreakpointIterator it, bool enable, bool setDisp, BreakpointDisposition disp)
{
  if (enable) {
    active_breakpoints.insert((*it)->address);
  } else {
    active_breakpoints.erase((*it)->address);
  }
  (*it)->enabled = enable;

  if (setDisp)
    (*it)->disposition = disp;

  return (*it)->id;
}

int UserBreakpoits::setEnabled(int id, bool enable, bool setDisp, BreakpointDisposition disp)
{
  BreakpointIterator it = lookupI(id);
  if (it == breakpoints.end()) {
    BreakpointsUI::no_breakpoint(id);
    return -1;
  }

  return setEnabled(it, enable, setDisp, disp);
}

int UserBreakpoits::setIgnoreCount(int id, int count)
{
  BreakpointIterator it = lookupI(id);
  if (it == breakpoints.end()) {
    BreakpointsUI::no_breakpoint(id);
    return -1;
  }

  (*it)->ignore_count = count;
  BreakpointsUI::ignore_success(id, count);

  return id;
}

int UserBreakpoits::check(uint16_t address)
{
  // This must be efficient (used each cycle).
  // First make a quick check
  if (active_breakpoints.count(address)) {
    // there sould be a breakpoint, look it up
    BreakpointIterator it;

    for (it = breakpoints.begin();
	 it != breakpoints.end();
	 it++) {
      if ((*it)->address == address &&
	  (*it)->enabled) {
	// return here: Only single breakpoint per address supported (if more are needed active_breakpoints has to be more complicated)
	Breakpoint *b = *it;
	bool temporary = false;

	b->hits++;
	if (b->ignore_count) {
	  b->ignore_count--;
	  return 0;
	} else {
	  switch (b->disposition) { 
	  case Keep:
	    // Fine
	    break;
	  case Disable:
	    this->setEnabled(it, false, true, Disable);
	    break;
	  case Delete:
	    temporary = true;
	    break;
	  default:
	    fprintf(stderr, "internal inconsistency: Unknown disposition");
	  }
	  BreakpointsUI::hit(b->id, b->address, temporary, b->file.c_str(), b->line);
	  if (temporary) 
	    this->erase(it);
	  return b->id;
	}
      }
    }
  }

  return 0;
}

void UserBreakpoits::showInfo()
{
  BreakpointIterator it;
  bool not_empty = breakpoints.begin() != breakpoints.end();
  BreakpointsUI::list_header(not_empty);

  for (it = breakpoints.begin();
      it != breakpoints.end();
      it++) {
    BreakpointsUI::list_line(*it);
  }
}

BreakpointIterator UserBreakpoits::lookupA(uint16_t address)
{
    BreakpointIterator it;

    for (it = breakpoints.begin();
	 it != breakpoints.end();
	 it++) {
      if ((*it)->address == address) {
	return it;
      }
    }
    
    return breakpoints.end();
}

BreakpointIterator UserBreakpoits::lookupI(int id)
{
    BreakpointIterator it;

    for (it = breakpoints.begin();
	 it != breakpoints.end();
	 it++) {
      if ((*it)->id == id) {
	return it;
      }
    }
    
    return breakpoints.end();
}



// vim: sw=2 si:
