#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILE_COUNT 10
#define LINE_SIZE 512

#define DEBUG_PREFIX ".debug "


int ommit_debug_lines = 0;

const char *asmName;
const char *outputName;
FILE *asmFile;
FILE *outputFile;
char *srcNames[MAX_FILE_COUNT] = { 0, };
FILE *srcFiles[MAX_FILE_COUNT] = { 0, };
char asmLine[LINE_SIZE];
char srcLine[LINE_SIZE];
int srcLastLineNo[LINE_SIZE] = {0,};
int srcCount = 0;

void OutputSource(int fileId, int untilLine)
{
	char *p;
	int i = srcLastLineNo[fileId];

	while (i < untilLine &&
			fgets (srcLine, sizeof srcLine, srcFiles[fileId] ) != NULL ) {
		i++;
		// skip whitespace to ommit epty lines
		p = srcLine;
		while (isspace(*p)) {
			p++;
		}
		// is the line not empty
		if (*p) {
			fprintf(outputFile, ";|%s:%-3d| %s", srcNames[fileId], i, srcLine);
		}
	}
	srcLastLineNo[fileId] = i;
}

int main (int argc, char **argv)
{
	int i;
	const int debug_prefix_size = strlen(DEBUG_PREFIX);
	int argStart = 1;

	if (argc > 2 && argv[1][0] == '-') {
		if (argv[1][1] == 'g') {
			argStart++;
			ommit_debug_lines = argv[1][2] <= '1';
		} else {
			printf("Unrecognized option: %s\n", argv[1]);
		}
	}

	if (argc != argStart+2) {
		printf("Usage: %s [-glevel] ASM_FILE_WITH_DEBUG_INFO OUTPUT_FILE\n"
			"   -glevel: if level < 2, the original debug information is removed (replaced by the C source)\n", argv[0]);
		return 1;
	}

	asmName = argv[argStart++];
	outputName = argv[argStart];

	asmFile = fopen (asmName, "r" );
	if ( asmFile == NULL )
	{
		perror ( asmName ); // report file open error
		return 2;
	}

	outputFile = fopen (outputName, "w" );
	if ( outputFile == NULL )
	{
		perror ( outputName ); // report file open error
		return 2;
	}

	while ( fgets (asmLine, sizeof asmLine, asmFile ) != NULL ) /* read a line */
	{
		if (strncmp(asmLine, DEBUG_PREFIX, debug_prefix_size) == 0) {
			char *subdirective = asmLine+debug_prefix_size;
			char *args = NULL;

			if (!ommit_debug_lines) {
				// Also output the original debug lines
				fputs (asmLine, outputFile );
			}

			if (strncmp(subdirective, "file ", 5) == 0) {
				// .debug file FILE_ID:FILE_NAME.c
				int id;
				char *fname;

				args = subdirective + 5;
				if (sscanf(args, "%d:%s\n", &id, srcLine) != 2) {
					fprintf(outputFile, "; ERROR: wrong format of debug directive arguments:\n");
					fputs (asmLine, outputFile);
				} else if (id != srcCount+1) {
					fprintf(outputFile, "; ERROR: Unexpected file id (%d) should be %d:\n", id, srcCount+1);
					fputs (asmLine, outputFile);
				} else {
					FILE *f= fopen(srcLine, "r");
					if (f == NULL) {
						fprintf(outputFile, "; ERROR: Unable to open \"%s\" file :\n", srcLine);
						fputs (asmLine, outputFile);
					} else {
						srcFiles[srcCount] = f;
						srcNames[srcCount] = strdup(srcLine);
						srcCount++;
					}
				}
			} else if (strncmp(subdirective, "line ", 5) == 0) {
				// .debug line FILE_ID:LINE_NUM
				int id;
				int line;

				args = subdirective + 5;
				if (sscanf(args, "%d:%d\n", &id, &line) != 2) {
					fprintf(outputFile, "; ERROR: wrong format of debug directive arguments:\n");
					fputs (asmLine, outputFile);
				} else if (id <= 0 || id > srcCount) {
					fprintf(outputFile, "; ERROR: Unexpected file id (%d) should be less in [1,%d]:\n", id, srcCount);
					fputs (asmLine, outputFile);
				} else {
					OutputSource(id-1, line);
				}
			}
		} else if (ommit_debug_lines &&
				strncmp(asmLine, "; " DEBUG_PREFIX, 2+debug_prefix_size) == 0) {
			// Ommit debug comments.
		} else {
			// Not a special line, just output
			fputs (asmLine, outputFile );
		}
	}
	fclose (asmFile);
	fclose (outputFile);

	for (srcCount--; srcCount >= 0; srcCount--) {
		fclose(srcFiles[srcCount]);
		free(srcNames[srcCount]);
	}

	return 0;
}

