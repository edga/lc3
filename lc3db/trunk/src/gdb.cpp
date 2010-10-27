/*\
 *  LC-3 Simulator
 *  Copyright (C) 2004  Anthony Liguori <aliguori@cs.utexas.edu>
 *  Copyright (C) 2004  Ehren Kret <kret@cs.utexas.edu>
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

uint16_t load_prog(const char *file, Memory &mem)
{
  std::string object = file;
  std::string base = object.substr(0, object.rfind(".obj"));
  std::string debug = base + ".dbg";
  uint16_t ret = mem.load(object);
  FILE *f = (ret != 0xFFFF) ? fopen(debug.c_str(), "r") : 0;
  char source[4096];
  int linenum;
  int addr;

  if (!f) return ret;

  fgets(source, sizeof(source), f);
  source[strlen(source) - 1] = 0;

  while (!feof(f)) {
    if (fgetc(f) != '!') {
      if (2 == fscanf(f, "%d:%x\n", &linenum, &addr)) {
	char buf[10];
	sprintf(buf, "%d", linenum);
	std::string str = source;
	str += ":";
	str += buf;
	str += ":0";
	mem.debug[addr & 0xFFFF] = str;
      }
    } else {
      char label[4096];
      if (2 == fscanf(f, "%x:%s\n", &addr, label)) {
	mem.symbol[label] = addr;
      }
    }
  }

  return ret;
}

int gdb_mode(LC3::CPU &cpu, Memory &mem, Hardware &hw,
	     bool gui_mode, bool quiet_mode)
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
  std::set<uint16_t> tbreakpoints;

  if (gui_mode) {
    printf(MARKER "%s:beg:0x%.4x\n", 
	   mem.debug[cpu.PC].c_str(), cpu.PC & 0xFFFF);
  } else if (!quiet_mode) {
    printf("0x%.4x: %.4x: ", cpu.PC & 0xFFFF, mem[cpu.PC] & 0xFFFF);
    cpu.decode(mem[cpu.PC]);
  }

  while ((cmd = readline(quiet_mode ? "(gdb) " : "(gdb) "))) try {
    int instructions_to_run = 0;

    if (!*cmd) cmd = last_cmd.c_str();
#if defined(USE_READLINE)
    add_history(cmd);
#endif

    std::istringstream incmd(cmd);
    std::string cmdstr;
    
    incmd >> cmdstr;

    if (cmdstr == "run") {
      incmd >> param1 >> param2;
      if (param1 == "<" && !param2.empty()) {
	hw.set_tty(open(param2.c_str(), O_RDWR));
      }

      uint16_t pc = load_prog("lib/los.obj", mem);
      if (pc == 0xFFFF) {
	sprintf(sys_string, "%s/lib/lc3db/los.obj", path_ptr);
	printf("Loading %s\n", sys_string);
	pc = load_prog(sys_string, mem);
      }
      else {
	printf("Loading lib/los.obj\n");
      }

      if (pc != 0xFFFF) {
	cpu.PC = mem[0x01FF];
	cpu.PSR = 0x0000;
	instructions_to_run = 0x7FFFFFFF;
	mem[0xFFFE] = mem[0xFFFE] | 0x8000;
      } else {
	printf("Could not find los.obj\n");
      }
    } else if (cmdstr == "force" || cmdstr == "f") {
      incmd >> param1 >> param2;
      off = lexical_cast<uint16_t>(param2);
      off&= 0xFFFF;
	
      if(mytable.count(param1.c_str()))
	*mytable[param1.c_str()] = off;
      else if(mem.symbol.count(param1)) {
	uint16_t addr = mem.symbol[param1];
	mem[addr] = off;
      } else {
	uint16_t off2 = lexical_cast<uint16_t>(param1);
	mem[off2] = off;
      }
    } else if (cmdstr == "set") {
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
      printf("foo\n");
      instructions_to_run = 0x7FFFFFFF;
    } else if (cmdstr == "disassemble" || cmdstr == "dasm") {
      incmd >> param1 >> param2;
      uint16_t start = lexical_cast<uint16_t>(param1);
      uint16_t end  = lexical_cast<uint16_t>(param2);
      for (; start < end; start++) {
	uint16_t IR = mem[start];
	for (std::map<std::string, uint16_t>::iterator i = mem.symbol.begin();
	     i != mem.symbol.end(); ++i) {
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
      uint16_t pc = load_prog(param1.c_str(), mem);
      if (pc != 0xFFFF) {
	cpu.PC = pc;
	const char *file = "";
	if (!mem.debug[cpu.PC].empty()) {
	  file = mem.debug[cpu.PC].c_str();
	}
	if (gui_mode) {
	  printf(MARKER "%s:beg:0x%.4x\n", 
		 mem.debug[cpu.PC].c_str(), cpu.PC & 0xFFFF);
	} else if (!quiet_mode) {
	  printf("0x%.4x: %.4x: ", cpu.PC & 0xFFFF, mem[cpu.PC] & 0xFFFF);
	  cpu.decode(mem[cpu.PC]);
	}
	mem[0xFFFE] = mem[0xFFFE] | 0x8000;
      } else {
	printf("Could not open %s\n", param1.c_str());
      }
    } else if (cmdstr == "next" || cmdstr == "n") {
      uint16_t IR = mem[cpu.PC];

      switch (IR & 0xF000) {
      case 0xF000: // TRAP
      case 0x4000: // JSR
	tbreakpoints.insert(cpu.PC + 1);
	instructions_to_run = 0x7FFFFFFF;
	break;
      default:
	instructions_to_run = 1;
      }
    } else if (cmdstr == "break" || cmdstr == "b") {
      uint16_t bp_addr;
      incmd >> param1;

      if (mem.symbol.count(param1)) {
	bp_addr = mem.symbol[param1];
      } else {
	bp_addr = lexical_cast<uint16_t>(param1);
      }
      printf("Setting breakpoint at 0x%.4x\n", bp_addr);
      tbreakpoints.insert(bp_addr);

    } else if (cmdstr == "print" || cmdstr == "p" || cmdstr == "output") {
      incmd >> param1;

      if (mytable.count(param1.c_str())) {
	printf("%s = %.4x (%d)\n", param1.c_str(),
	       *mytable[param1.c_str()] & 0xFFFF,
	       *mytable[param1.c_str()] & 0xFFFF);
      } else if (mem.symbol.count(param1)) {
	uint16_t addr = mem.symbol[param1];
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
    } else if (cmdstr == "step" || cmdstr == "s") {
      param1.clear();
      incmd >> param1;
      if (!param1.empty()) try {
	instructions_to_run = lexical_cast<uint16_t>(param1);
      } catch(bad_lexical_cast &e) {
	instructions_to_run = 1;
      } else {
	instructions_to_run = 1;
      }
    } else if (cmdstr == "exit" || cmdstr == "quit" || cmdstr == "q") {
      break;
    } else if (cmdstr == "info") {
    } else {
      printf("Bad command `%s'\nTry using the `help' command.\n", cmd);
    }

    while (instructions_to_run) {
      instructions_to_run--;

      if (!(mem[0xFFFE] & 0x8000) || tbreakpoints.count(cpu.PC)) {
	tbreakpoints.erase(cpu.PC);
	instructions_to_run = 0;
      } else {
	cpu.cycle();
	mem.cycle();
	instruction_count++;
      }

      if (instructions_to_run == 0) {
	if (gui_mode) {
	  printf(MARKER "%s:beg:0x%.4x\n", 
		 mem.debug[cpu.PC].c_str(), cpu.PC & 0xFFFF);
	} else if (!quiet_mode) {
	  printf("0x%.4x: %.4x: ", cpu.PC & 0xFFFF, mem[cpu.PC] & 0xFFFF);
	  cpu.decode(mem[cpu.PC]);
	}
      }
    }

    last_cmd = cmd;
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
