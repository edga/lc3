/*\
 *  LC-3 Simulator
 *  Copyright (C) 2004  Anthony Liguori <aliguori@cs.utexas.edu>
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
 *
 * vim: sw=2 si:
\*/

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "cpu.hpp"
#include "memory.hpp"
#include "hardware.hpp"
#include "source_info.hpp"

extern "C" {
  int lc3_asm(int, const char **);
}

char* path_ptr;
int gdb_mode(LC3::CPU &cpu, SourceInfo &src_info, Memory &mem, Hardware &hw,
	     bool gui_mode, bool quiet_mode, const char *exec_file);
uint16_t load_prog(const char *file, SourceInfo &src_info, Memory &mem, uint16_t *entry);

const char * PROGRAM = "LC-3 Simulator 1.0";
const char * COPYRIGHT =
"Copyright 2004 Anthony Liguori <aliguori@cs.utexas.edu>";
const char * INFO =
"The LC-3 Simulator is free software, covered by the GNU General Public\n"
"License, and you are welcome to change it and/or distribute copies of it\n"
"under certain conditions.\n"
"See the COPYING file included in this distribution.\n"
"There is absolutely no warranty for the LC-3 Simulator.";

int main(int argc, char **argv) 
{
  Memory mem;
  SourceInfo src_info;
  struct option longopts[] = {
    {"fullname", 0, 0, 'f'},
    {"cd"      , 1, 0, 'C'},
    {"help"    , 0, 0, 'h'},
    {"version" , 0, 0, 'V'},
    {"initddd" , 0, 0, 'I'},
    {"rootdir" , 1, 0, 'r'},
    {"compile" , 1, 0, 'c'},
    {"emacs"   , 0, 0, 'E'},
    {"ddd"     , 0, 0, 'D'},
    {"quiet"   , 0, 0, 'q'},
    {NULL      , 0, 0, 0}
  };
  const char *loc_path_ptr;
  int ch;
  char sys_string[2048];
  int index = 0;
  bool gui_mode = false;
  bool quiet_mode = false;

  loc_path_ptr = getenv("LC3DB_ROOT");
  if(loc_path_ptr == NULL) {
    loc_path_ptr = PREFIX;
  }
  path_ptr = strdup(loc_path_ptr);

  while (-1 != (ch = getopt_long_only(argc, argv, "c:r:EDI", longopts, &index))) {
    switch (ch) {
    case 'f':
      gui_mode = true;
      break;
    case 'q':
      quiet_mode = true;
      break;
    case 'C':
      break;
    case 'c':
      {
	const char *args[] = {
	  argv[0],
	  optarg,
	  NULL
	};
	return lc3_asm(2, args);
	break;
      }
    case 'h':
      printf("Usage: %s [-c filename.asm]\n"
	     "Runs an interactive simulator of the LC-3 architecture.\n"
	     "\n"
	     "  -c, --compile=filename   compile filename and exit\n"
	     "  -E, --emacs              launch emacs as a front end\n"
	     "  -D, --ddd                launch ddd as a front end\n"
	     "  -h, --help               displays this help screen\n"
	     "  -I, --initddd            reinitialize ddd display init (rm -rf ~/.lc3db)\n"
	     "  -r, --rootdir            root directory for files needed by lc3db\n"
	     "\n"
	     "When no options are specified, enter the interactive simulator\n"
	     "mode.\n"
	     "\n"
	     "Report bugs to <edgar.lakis@gmail.com>.\n"
	     , *argv);
      exit(0);
      break;
    case 'V':
      printf("%s\n%s\n", PROGRAM, COPYRIGHT);
      exit(0);
      break;
    case 'I':
      {
	char buffer[1024];
	sprintf(buffer, "%s/.lc3db", getenv("HOME"));
	if (access(buffer, R_OK | X_OK)) {
	  printf("%s does not exist\n", buffer);
	}
	else {
	  sprintf(sys_string, "/bin/rm -rf ~/.lc3db");
	  printf("/bin/rm -rf %s\n", buffer);
	 system(sys_string);
	}
	exit(0);
	break;
      }
    case 'r':
      {
	path_ptr = strdup(argv[2]);
	break;
      }
    case 'E':
      {
	char buffer[1024];
	sprintf(buffer, "emacs --eval '(gud-gdb \"%s -q -f %s\")'", *argv, argv[optind]);
	printf("%s\n", buffer);
	return system(buffer);
      }
    case 'D':
      {
	char buffer[4096];

	sprintf(buffer, "%s/.lc3db", getenv("HOME"));

	// // This is a hack so that one always starts with the top level init file
	// sprintf(sys_string, "/bin/rm -rf ~/.lc3db");
	// system(sys_string);

	if (access(buffer, R_OK | X_OK)) {
	  if (!access("ddd/init", R_OK)) {
	    printf("Copying files from ./ddd to ~/.lc3db\n");
	    system("rm -rf ~/.lc3db; cp -rf ddd ~/.lc3db");
	  } else {
	    printf("Copying files from %s/share/lc3db/ddd to ~/.lc3db\n", path_ptr);
	    sprintf(sys_string, "rm -rf ~/.lc3db; cp -rf %s/share/lc3db/ddd ~/.lc3db",
		    path_ptr);
	    system(sys_string);
	  }
	}
	setenv("DDD_STATE", buffer, 1);
	if (optind < argc) {
	  sprintf(sys_string, "ddd --debugger %s/bin/lc3db '%s'", path_ptr, argv[optind]);
	} else {
	  sprintf(sys_string, "ddd --debugger %s/bin/lc3db", path_ptr);
	}
	printf("%s\n", sys_string);
	return system(sys_string);
      }
    default:
      break;
    }
  }

  if (0xFFFF == load_prog("lib/los.obj", src_info, mem, NULL)) {
    sprintf(sys_string, "%s/lib/lc3db/los.obj", path_ptr);
    printf("Loading %s\n", sys_string);
    load_prog(sys_string, src_info, mem, NULL);
  }
  else {
    printf("Loading lib/los.obj\n");
  }

  LC3::CPU cpu(mem);
  Hardware hw(mem, cpu);

  if (!quiet_mode) {
    printf("%s\n%s\n%s\n", PROGRAM, COPYRIGHT, INFO);
  }

  // Terminal creation disabled, because it is build into ddd
  if (0){
    // Create a terminal
    FILE *tmpF = popen("3>&1 xterm -title 'LC3 terminal' -e sh -c 'tty 1>&3; sleep 100000'", "r");
    if (tmpF) {
      char buf[256];
      char *ret;
      ret = fgets(buf, sizeof(buf), tmpF);
      if (ret) {
	while(*ret){
	  if (*ret == '\n' || *ret == '\r') {
	    *ret = 0;
	    break;
	  }
	  ret++;
	}
    	fprintf(stderr, "tty: %s\n", buf);
	hw.set_tty(open(buf, O_RDWR));
      }
    } else {
	fprintf(stderr, "popen failed\n");      
    }
  }

  char * exec_file = NULL;
  // Load user specified code
  if (optind < argc) {
    exec_file = argv[optind];
    int l = strlen(exec_file);
    if (l < 4 || strcasecmp(&exec_file[l-4], ".obj")) {
   	fprintf(stderr, "object file expected (\"%s\" given)\n", exec_file);
	exec_file = NULL;
    }
  }

  return gdb_mode(cpu, src_info, mem, hw, gui_mode, quiet_mode, exec_file);
}
