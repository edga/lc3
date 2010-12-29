/*\
 *  LC-3 Simulator
 *  Copyright (C) 2004  Anthony Liguori <aliguori@cs.utexas.edu>
 *  Copyright (C) 2004  Ehren Kret <kret@cs.utexas.edu>
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

#include <stdio.h>
#include <signal.h>
#if defined(USE_READLINE)
#include <readline/readline.h>
#include <readline/history.h>
#else
static char* readline (const char* prompt);
#endif
#include <set>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "cpu.hpp"
#include "memory.hpp"
#include "hardware.hpp"
#include "lexical_cast.hpp"
#include "source_info.hpp"
#include "breakpoints.hpp"

  extern char* path_ptr;

namespace std {
  struct ichar_traits : std::char_traits<char> {
    static int compare(const char *lhs, const char *rhs, size_t len) {
      return strncasecmp(lhs, rhs, len);
    }
  };
  typedef basic_string<char, ichar_traits, allocator<char> > istring;
}

const char * HELP =
"Commands: - shortcuts shown in ()\n"
" compile <filename.asm>\n"
"   Assembles filename.asm.\n\n"

" tty <terminal_device>\n"
"   Use pseudoterminal instead of stdin/stdout.\n\n"

" break (b) <addr|sym>\n"
"   Set breakpoint at address <addr> or at the label indicated by sym.\n\n"

" continue (c)\n"
"   Executions instructions until halt.\n\n"

" disassemble (dasm) <start> <end>\n"
"   Disassemble insructions from address <start> to <end>.\n\n"

" dump (d) <start> <end>\n"
"   Dump the memory from address start to end.\n\n"

" finish\n"
"   Continue untill return\n\n"

" force (f) <symbol> <value>\n"
"   Sets the data represented by symbol to value\n\n"

" load (l) <filename.obj>\n"
"   Loads a file named filename.obj. It will also attempt to read a debug\n"
"   named filename.dbg.  The PC will be set to where the file was loaded.\n\n"

" next (n)\n"
"   Steps over the next instruction (useful for TRAP and JSR commands)\n\n"

" print (p) <data>\n"
"   Prints the value of the symbol, register, or address specified by\n"
"   data.  Use \"print all\" to see contents of registers.\n\n"

" run\n"
"   Initializes the machines and runs the operating system.\n\n"

" step (s) [num]\n"
"   Executes the next num instruction.\n\n"

" quit (q) exit\n"
"   Quits the simulation session.\n\n"

" x regs\n"
"   Examine contents of registers.\n\n"

" help (h)\n"
"   Displays this help screen\n";

#define MARKER "\x1a\x1a"

extern "C" {
  int lc3_asm(int, const char **);
}

uint16_t load_prog(const char *file, SourceInfo &src_info, Memory &mem, uint16_t *pEntry)
{
  char buf[4096];
  int debugLine;
  int linenum;
  int fileId = -1;
  int addr, lastAddr;
  int c;
  FILE *f;
  uint16_t ret;
  char *entryLabel = NULL;
  std::string object = file;
  std::string base = object.substr(0, object.rfind(".obj"));
  std::string debug = base + ".dbg";
 
  ret  = mem.load(object);
  if (ret == 0xFFFF) return ret;

  f = fopen(debug.c_str(), "r");
  if (!f) return ret;

  if (1 == fscanf(f, "ENTRY:%s\n", buf)) {
    entryLabel = strdup(buf);
  } else {
    fprintf(stderr, "ENTRY label missing (will break on first word)\n");
  }

  debugLine = 0;
  while (!feof(f)) {
    debugLine++;
    c = fgetc(f);

    switch(c) {
    case '#':
      if (1 == fscanf(f, "%d:", &fileId)) {
        fgets(buf, sizeof(buf), f);
        buf[strlen(buf) - 1] = 0;
        src_info.add_source_file(fileId, std::string(buf));
      } else {
        fprintf(stderr, "Wrong line format in debug information file: %s (line: %d)\n", debug.c_str(), debugLine);
      }
      break;

    case '!':
      if (2 == fscanf(f, "%x:%s\n", &addr, buf)) {
        src_info.symbol[buf] = addr;
      } else {
        fprintf(stderr, "Wrong line format in debug information file: %s (line: %d)\n", debug.c_str(), debugLine);
      }
      break;

    case '@':
      if (4 == fscanf(f, "%d:%d:%x:%x\n", &fileId, &linenum, &addr, &lastAddr)) {
	src_info.add_source_line(addr & 0xFFFF, lastAddr & 0xFFFF, fileId, linenum);
      } else {
        fprintf(stderr, "Wrong line format in debug information file: %s (line: %d)\n", debug.c_str(), debugLine);
      }
      break;

    default:
        fprintf(stderr, "Unrecognised line type in debug information file: %s (line: %d)\n", debug.c_str(), debugLine);
        /* read reminder of the line */
        fgets(buf, sizeof(buf), f);
    }
  }

  if (pEntry) {
    if (entryLabel && src_info.symbol.count(entryLabel)) {
      *pEntry = src_info.symbol[entryLabel];
    } else {
      *pEntry = ret;
    }
  }

  return ret;
}

static int signal_received = 0;
void sigproc(int sig)
{
  signal(SIGINT, sigproc); /* reset for portability */
  fprintf(stderr, "<Interrupted>\n");
  signal_received = 1;
  // TODO: make proper synchronization to avoid race conditions
}

void show_execution_position(LC3::CPU &cpu, SourceInfo &src_info, Memory &mem, bool gui_mode, bool quiet_mode)
{
  if (gui_mode) {
    SourceLocation sl = src_info.find_source_location_absolute(cpu.PC);
    if (sl.lineNo > 0) {
      printf(MARKER "%s:%d:0:%s:0x%.4x\n",
          //mem.debug[cpu.PC].c_str(), cpu.PC & 0xFFFF);
          sl.fileName, sl.lineNo,
          (sl.isHLLSource && cpu.PC > sl.firstAddr) ? "middle" : "beg",
              cpu.PC & 0xFFFF);
    } else {
      printf("%.4x in <unknown>\n", cpu.PC & 0xFFFF);
    }
  } else if (!quiet_mode) {
    printf("0x%.4x: %.4x: ", cpu.PC & 0xFFFF, mem[cpu.PC] & 0xFFFF);
    cpu.decode(mem[cpu.PC]);
  }
}

int gdb_mode(LC3::CPU &cpu, SourceInfo &src_info, Memory &mem, Hardware &hw,
	     bool gui_mode, bool quiet_mode, const char *exec_file)
{
  if (!quiet_mode) {
    printf("Type `help' for a list of commands.\n");
  }

  std::map<std::istring, int16_t *> mytable;
  mytable["PC"] = (int16_t *)&cpu.PC;
  mytable["R0"] = &cpu.R[0];
  mytable["R1"] = &cpu.R[1];
  mytable["R2"] = &cpu.R[2];
  mytable["R3"] = &cpu.R[3];
  mytable["R4"] = &cpu.R[4];
  mytable["R5"] = &cpu.R[5];
  mytable["R6"] = &cpu.R[6];
  mytable["R7"] = &cpu.R[7];
  mytable["PSR"] = (int16_t *)&cpu.PSR;
  std::string param1;
  std::string param2;
  uint16_t off = 0;
  uint16_t x_addr, x_off;
  char x_string[256];
  char sys_string[2048];

  int instruction_count = 0;

#if defined(USE_READLINE)
  using_history();
#endif
  mem[0xFFFE] = mem[0xFFFE] | 0x8000;

  const char *cmd = 0;
  std::string last_cmd;

  UserBreakpoits breakpoints(src_info);
  bool break_on_return;
  bool breakpoint_hit = false;	// Flag set once breakpoint is detected, so that breakpointed instruction can be enabled.
  int temporary_breakpoint = -1;
  int repeat_count = 0;
  const int INT_INFINITY = 0x7FFFFFFF;
  int saved_execution_range_start = 0;
  int saved_execution_range_end = INT_INFINITY;

  show_execution_position(cpu, src_info, mem, gui_mode, quiet_mode);

  signal(SIGINT, sigproc);
  break_on_return = false;
  while ((cmd = readline(quiet_mode ? "(gdb) " : "(gdb) "))) try {
    int instructions_to_run = 0;
    int limit_execution_range_start = 0;
    int limit_execution_range_end = INT_INFINITY;
    int step_over_calls = 0;
    int in_step_over_mode = 0;

    if (!*cmd) cmd = last_cmd.c_str();
#if defined(USE_READLINE)
    add_history(cmd);
#endif

    std::istringstream incmd(cmd);
    std::string cmdstr;
   
    cmdstr.clear(); 
    param1.clear();
    param2.clear();
    incmd >> cmdstr;

    if (cmdstr == "run") {
      incmd >> param1 >> param2;
      if (param1 == "<" && !param2.empty()) {
	hw.set_tty(open(param2.c_str(), O_RDWR));
      }

      uint16_t pc = load_prog("lib/los.obj", src_info, mem, NULL);
      if (pc == 0xFFFF) {
	sprintf(sys_string, "%s/lib/lc3db/los.obj", path_ptr);
	printf("Loading %s\n", sys_string);
	pc = load_prog(sys_string, src_info, mem, NULL);
      }
      else {
	printf("Loading lib/los.obj\n");
      }

      // Load executable if specified
      if (exec_file) {
	uint16_t entry;
	printf("Loading %s\n", exec_file);
      	uint16_t start_addr = load_prog(exec_file, src_info, mem, &entry);
	if (0xFFFF == start_addr) {
	  printf("failed to load %s\n", exec_file);
	} else {
	  mem[0x01FE] = start_addr;
	  int bt_id = breakpoints.add(entry, false);
	  //temporary_breakpoint = start_addr;
	}
      }

      if (pc != 0xFFFF) {
	cpu.PC = mem[0x01FF];
	cpu.PSR = 0x0000;
	instructions_to_run = INT_INFINITY;
	mem[0xFFFE] = mem[0xFFFE] | 0x8000;
      } else {
	printf("Could not find los.obj\n");
      }
    } else if (cmdstr == "finish") {
      break_on_return = true;
      instructions_to_run = INT_INFINITY;
    } else if (cmdstr == "force" || cmdstr == "f") {
      incmd >> param1 >> param2;
      off = lexical_cast<uint16_t>(param2);
      off&= 0xFFFF;
	
      if(mytable.count(param1.c_str()))
	*mytable[param1.c_str()] = off;
      else if(src_info.symbol.count(param1)) {
	uint16_t addr = src_info.symbol[param1];
	mem[addr] = off;
      } else {
	uint16_t off2 = lexical_cast<uint16_t>(param1);
	mem[off2] = off;
      }
    } else if (cmdstr == "set") {
      fprintf(stderr, "\"set\" command is not supported yet (try using \"force\")\n");
    } else if (cmdstr == "dump" || cmdstr == "d"  || cmdstr == "x") {
      if (cmdstr == "x") {
	incmd >> param2 >> param1;
	if (param2 == "regs" || param1 == "regs") {
	  char reg[] = "R0";
	  for (int i = 0; i < 8; i++) {
	    if (i == 4) printf("\n");
	    reg[1] = '0' + i;
	    printf("R%d:  %.4x (%5d)  ", i,
		   cpu.R[i] & 0xFFFF, cpu.R[i] & 0xFFFF);
	  }
	  printf("\n");
	  printf(
      "PC:  %.4x (%5d)  PSR: %.4x (%5d)  MCR: %.4x (%5d)  MCC: %.4x (%5d)\n"
      "Instructions Run: %5d               Mode: %s  Pri: %d  NZP: %c%c%c\n",
		 cpu.PC & 0xFFFF, cpu.PC & 0xFFFF,
		 cpu.PSR & 0xFFFF, cpu.PSR & 0xFFFF,
		 mem[0xFFFE] & 0xFFFF, mem[0xFFFE] & 0xFFFF,
                 mem[0xFFFF] & 0xFFFF, mem[0xFFFF] & 0xFFFF,
		 instruction_count,
		 (cpu.PSR & 0x8000) ? "User  " : "Kernel",
		 (cpu.PSR >> 8) & 0x7, (cpu.PSR&0x4)?'1':'0',
                 (cpu.PSR&0x2)?'1':'0', (cpu.PSR&0x1)?'1':'0');
	  continue;
	}

	x_addr = lexical_cast<uint16_t>(param1);
	x_off = lexical_cast<uint16_t>(param2);
	x_addr = x_addr + x_off;
	sprintf(x_string, "x%x", x_addr);
	param2 = x_string;

      } else {
	incmd >> param1 >> param2;
      }
      off = lexical_cast<uint16_t>(param1);
      uint16_t off2 = lexical_cast<uint16_t>(param2);
      uint16_t counter = 1;

      std::ostringstream myout;
      myout << std::setprecision(4) << std::setw(4) << std::setfill('0')
	    << std::hex << off << ":";
      for(;off<=off2;off++,counter++) {
	myout << " " << std::hex << std::setprecision(4) << std::setw(4) 
	      << std::setfill('0') << mem[off] ;
	if(counter%8==0 && off!=off2) {
	  myout << std::endl << std::hex << std::setprecision(4) 
		<< std::setw(4) << std::setfill('0') << off+1 << ":";
	  counter=0;
	}
      }
      myout << std::endl;
      printf("%s",myout.str().c_str());
    } else if (cmdstr == "compile") {
      incmd >> param1;
      const char *args[] = { "", param1.c_str(), NULL };
      lc3_asm(2, args);
    } else if (cmdstr == "tty") {
      incmd >> param1;
      hw.set_tty(open(param1.c_str(), O_RDWR));
    } else if (cmdstr == "continue" || cmdstr == "c" || cmdstr=="cont") {
      printf("running\n");
      instructions_to_run = INT_INFINITY;
    } else if (cmdstr == "disassemble" || cmdstr == "dasm") {
      incmd >> param1 >> param2;
      uint16_t start = lexical_cast<uint16_t>(param1);
      uint16_t end  = lexical_cast<uint16_t>(param2);
      for (; start < end; start++) {
	uint16_t IR = mem[start];
	for (std::map<std::string, uint16_t>::iterator i = src_info.symbol.begin();
	     i != src_info.symbol.end(); ++i) {
	  if (i->second == start) {
	    printf("%s:\n", i->first.c_str());
	    break;
	  }
	}
	printf("0x%.4x: %.4x: ", start & 0xFFFF, IR & 0xFFFF);
	cpu.decode(IR);
      }
    } else if (cmdstr == "help" || cmdstr == "h") {
      printf("%s", HELP);
    } else if (cmdstr == "load" || cmdstr == "l" || cmdstr == "file") {
      incmd >> param1;
      uint16_t entry;
      uint16_t pc = load_prog(param1.c_str(), src_info, mem, &entry);
      if (pc != 0xFFFF) {
	cpu.PC = pc;
	//const char *file = "";
	//if (!mem.debug[cpu.PC].empty()) {
	//  file = mem.debug[cpu.PC].c_str();
	//}
	int bt_id = breakpoints.add(entry, false);
	show_execution_position(cpu, src_info, mem, gui_mode, quiet_mode);
	mem[0xFFFE] = mem[0xFFFE] | 0x8000;
      } else {
	printf("Could not open %s\n", param1.c_str());
      }
    } else if (cmdstr == "stepi" || cmdstr == "si") {
      incmd >> param1;
      try {
	instructions_to_run = lexical_cast<uint16_t>(param1);
      } catch(bad_lexical_cast &e) {
	instructions_to_run = 1;
      }
    } else if (cmdstr == "nexti" || cmdstr == "ni") {
  l_handle_nexti:
      instructions_to_run = 1;
      step_over_calls = 1;
    } else if (cmdstr == "step" || cmdstr == "s") {
      incmd >> param1;
      int repeat;
      try {
	repeat = lexical_cast<uint16_t>(param1);
      } catch(bad_lexical_cast &e) {
	repeat = 1;
      }
      SourceLocation line = src_info.find_source_location_absolute(cpu.PC);
      if (line.lineNo <= 0 || !line.isHLLSource) {
	// No hi level line at current location
	instructions_to_run = repeat;
      } else {
	if (repeat != 1) {
	  fprintf(stderr, "step argument is currently not supported\n");
	}
	limit_execution_range_start = line.firstAddr;
	limit_execution_range_end = line.lastAddr;
	instructions_to_run = INT_INFINITY;
      }
    } else if (cmdstr == "next" || cmdstr == "n") {
      SourceLocation line = src_info.find_source_location_absolute(cpu.PC);
      if (line.lineNo <= 0 || !line.isHLLSource) {
	// No hi level line at current location
	goto l_handle_nexti;
      } else {
	instructions_to_run = INT_INFINITY;
	step_over_calls = 1;
	limit_execution_range_start = line.firstAddr;
	limit_execution_range_end = line.lastAddr;
      }
    } else if (cmdstr == "ignore") {
      incmd >> param1 >> param2;
      int id = lexical_cast<uint16_t>(param1);
      int count = lexical_cast<uint16_t>(param2);
      int res = breakpoints.setIgnoreCount(id, count);
      if (res > 0) {
	printf("Will ignore next %d crossings of breakpoint %d.\n", count, res);
      } else {
	printf("No breakpoint number %d.\n", id);
      }
    } else if (cmdstr == "break" || cmdstr == "b" ||
	       cmdstr == "tbreak" || cmdstr == "tb") {
      uint16_t bp_addr;
      bool bp_valid = false;
      size_t colPos;
      incmd >> param1;
      bool make_temporary = (cmdstr[0] == 't');

      if (src_info.symbol.count(param1)) {
	// Symbol
	bp_addr = src_info.symbol[param1];
	bp_valid = true;
      } else if ((colPos=param1.find(":")) != std::string::npos) {
	// FILENAME:LINE
	std::string fileName = param1.substr(0,colPos);
	try {
	  uint16_t lineNo;
	  lineNo = lexical_cast<uint16_t>(param1.substr(colPos+1));
	  bp_addr = src_info.find_line_start_address(fileName, lineNo);
	  bp_valid = bp_addr != 0;
	} catch(bad_lexical_cast &e) {
	  bp_valid = false;
	} 
      } else {
	// try verbatim number
	if (param1[0] == '*') {
	  bp_addr = lexical_cast<uint16_t>(param1.substr(1));
	} else {
	  bp_addr = lexical_cast<uint16_t>(param1);
	}
	bp_valid = true;
      }
      
      if (bp_valid) {
	int bt_id = breakpoints.add(bp_addr, make_temporary);
	//if (make_temporary && bt_id > 0) {
	//  breakpoints.setEnabled(bt_id, true, true, Delete);
	//}
      } else {
	printf("breakpoint specification [%s] is not valid\n", param1.c_str());
      }
	

    } else if (cmdstr == "print" || cmdstr == "p" || cmdstr == "output") {
      incmd >> param1;

      if (mytable.count(param1.c_str())) {
	printf("%s = %.4x (%d)\n", param1.c_str(),
	       *mytable[param1.c_str()] & 0xFFFF,
	       *mytable[param1.c_str()] & 0xFFFF);
      } else if (src_info.symbol.count(param1)) {
	uint16_t addr = src_info.symbol[param1];
	printf("0x%.4x: %.4x (%d)\n", addr & 0xFFFF,
	       mem[addr] & 0xFFFF, mem[addr] & 0xFFFF);
      } else if (param1 == "all") {
	std::map<std::istring, int16_t *>::iterator i;
	for (i = mytable.begin(); i != mytable.end(); ++i) {
	  printf("%s = %.4x (%d)\n", i->first.c_str(), 
		 (*i->second) & 0xFFFF, (*i->second) & 0xFFFF);
	}
      } else {
	off = lexical_cast<uint16_t>(param1);
	printf("0x%.4x: %.4x (%d)\n", off & 0xFFFF,
	       (int16_t)mem[off & 0xFFFF] & 0xFFFF,
	       (int16_t)mem[off & 0xFFFF] & 0xFFFF);
      }
    } else if (cmdstr == "exit" || cmdstr == "quit" || cmdstr == "q") {
      break;
    } else if (cmdstr == "ignore") {
      //param1.clear();
      //param2.clear();
      incmd >> param1 >> param2;
      int id = lexical_cast<uint16_t>(param1);
      int count = lexical_cast<uint16_t>(param1);
      breakpoints.setIgnoreCount(id, count);
    } else if (cmdstr == "delete" || cmdstr == "disable") {
      bool del = (cmdstr=="delete");
      //param1.clear();
      incmd >> param1;
      if (param1 == "breakpoints") {
	param1.clear();
	incmd >> param1;
      }
      if (param1.empty()) {
	printf("Breakpoint id expected.\nTry using the `help' command.\n");
      } else {
	do{
	  int id = lexical_cast<uint16_t>(param1);
	  if (del) {
	    breakpoints.erase(id);
	  } else {
	    breakpoints.setEnabled(id, false, false, Keep);
	  }
	  param1.clear();
	  incmd >> param1;
	} while (!param1.empty());
      }
    } else if (cmdstr == "enable") {
      BreakpointDisposition disp = Keep;
      //param1.clear();
      incmd >> param1;
      if (param1 == "breakpoints") {
	param1.clear();
	incmd >> param1;
      }
      if (param1 == "once") {
	disp = Disable;
	param1.clear();
	incmd >> param1;
      } else if (param1 == "delete") {
	disp = Delete;
	param1.clear();
	incmd >> param1;
      }
      if (param1.empty()) {
	printf("Breakpoint id expected.\nTry using the `help' command.\n");
      } else {
	do{
	  int id = lexical_cast<uint16_t>(param1);
	  breakpoints.setEnabled(id, true, disp != Keep, disp);
	  param1.clear();
	  incmd >> param1;
	} while (!param1.empty());
      }
    } else if (cmdstr == "info") {
      incmd >> param1;
      if (param1 == "breakpoints" || param1 == "b") {
	breakpoints.showInfo();
      }
    } else {
      printf("Bad command `%s'\nTry using the `help' command.\n", cmd);
      continue;
    }

    if (instructions_to_run) {
      while (instructions_to_run) {
	//fprintf(stderr, "\nIR: %d \tPC: %04x\n", instructions_to_run, cpu.PC & (0xFFFF));
	instructions_to_run--;

	// Check stoping conditions
	if (!(mem[0xFFFE] & 0x8000)) {
	  fprintf(stderr, "LC3 is halted\n");
	  break;
	}
	// FixMe:
	if (break_on_return) {
	  int i = mem[cpu.PC] & (0xFFFF);
	  if (i == 0xc1c0 || // RET
	      i == 0x8000)   // RTI
	  {
	    fprintf(stderr, "stopped on return\n");
	    break_on_return = false;
	    break;
	  }
	}
	if (signal_received) {
	  signal_received = 0;
	  break;
	}
	if (temporary_breakpoint == cpu.PC) {
	  temporary_breakpoint = -1;
	  if (!in_step_over_mode) {
	    break;
	  } else {
	    limit_execution_range_start = saved_execution_range_start;
	    limit_execution_range_end = saved_execution_range_end;
	    // Reset for next command
	    step_over_calls = 1;
	  }
	}
	if (!breakpoint_hit && breakpoints.check(cpu.PC)) {
	  breakpoint_hit = true;
	  break;
	}

	// next/nexti workaround
	if (step_over_calls) {
	  uint16_t IR_opcode = mem[cpu.PC] & 0xF000;
	  if (IR_opcode == 0xF000 || // TRAP
	      IR_opcode == 0x4000) { // JSR
	    // Next instruction is a call
	    in_step_over_mode = 1;
	    step_over_calls = 0;

	    temporary_breakpoint = cpu.PC + 1;

	    instructions_to_run = INT_INFINITY;
	    saved_execution_range_start = limit_execution_range_start;
	    saved_execution_range_end = limit_execution_range_end;
	    limit_execution_range_start = 0;
	    limit_execution_range_end = INT_INFINITY;
	  }
	}

	// Execute
	cpu.cycle();
	mem.cycle();
	instruction_count++;
	breakpoint_hit = false;

	if (cpu.PC < limit_execution_range_start ||
	    cpu.PC > limit_execution_range_end) {
	  break;
	}

      }

      show_execution_position(cpu, src_info, mem, gui_mode, quiet_mode);
    }

    last_cmd = cmd;
    repeat_count = 0;
  } catch (bad_lexical_cast &e) {
      printf("Bad command `%s'\nTry using the `help' command.\n", cmd);
  }

  return 0;
}

#if !defined(USE_READLINE)
static char*
readline (const char* prompt)
{
    char buf[200];
    char* strip_nl;

    /* Prompt and read a line until successful. */
    while (1) {

#if !defined(USE_READLINE)
	printf ("%s", prompt);
#endif
	/* read a line */
	if (fgets (buf, 200, stdin) != NULL)
	    break;

	/* no more input? */
    	if (feof (stdin))
	    return NULL;

    	/* Otherwise, probably a CTRL-C, so print a blank line and
	   (possibly) another prompt, then try again. */
    	puts ("");
    }

    /* strip carriage returns and linefeeds */
    for (strip_nl = buf + strlen (buf) - 1;
    	 strip_nl >= buf && (*strip_nl == '\n' || *strip_nl == '\r');
	 strip_nl--);
    *++strip_nl = 0;

    return strdup (buf);
}

#endif
// vim: sw=2 si:
