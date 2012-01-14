/* x86s running Linux */

#include <string.h>

static char rcsid[] = "$Id: lc3.c,v 1.1.1.1 2004/03/24 04:37:35 sjp Exp $";

#ifndef PATHSEP
#define PATHSEP "/"
#endif


#ifndef LCCDIR
#define LCCDIR "." PATHSEP
#endif

#ifndef BASEDIR
#define BASEDIR "." PATHSEP
#endif

/* char *suffixes[] = { ".c", ".i", ".s", ".o", ".out", 0 }; */
char *suffixes[] = { ".c", ".i", ".lcc", ".asm", ".obj", ".out", 0 };
char inputs[256] = "";
char *cpp[] = { LCCDIR "cpp",
	"-U__GNUC__", "-D__STDC__=1", "-D__STRICT_ANSI__", "-D__signed__=signed",
	"$1", "$2", "$3", 0 };
char *include[] = {"-I" LCCDIR "include", "-I" BASEDIR "lc3lib", 0 };
char *com[] = { LCCDIR "rcc", "-target=lc3", "$1", "$2", "$3", 0 };
char *as[] = { LCCDIR "lc3as", "$1", "$2", 0 };
char *ld[] = { LCCDIR "lc3pp", BASEDIR "lc3lib", "$1", "$2", "$3", 0 };
char *SourcePaste[] = { LCCDIR "SourcePaste", "$1", "$2", "$3", 0 };

extern char *concat(char *, char *);

int option(char *arg) {
  	if (strncmp(arg, "-lccdir=", 8) == 0) {
		include[0] = concat("-I", concat(&arg[8], PATHSEP "include"));
		cpp[0] = concat(&arg[8], PATHSEP "install" PATHSEP "cpp");
		com[0] = concat(&arg[8], PATHSEP "install" PATHSEP "rcc");
		ld[0]  = concat(&arg[8], PATHSEP "install" PATHSEP "lc3lib");
		ld[1]  = concat(&arg[8], PATHSEP "lc3lib");
	} else if (strncmp(arg, "-ld=", 4) == 0) {
		ld[0] = &arg[4];
	} else if (strncmp(arg, "-g", 2) == 0)
		return 1;
	else return 0;
	return 1;
}
