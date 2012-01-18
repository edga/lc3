/*
 * lc3Console.c
 *
 *  Created on: 11/10/2009
 *     Authors: Edgar <s081553>
 *      	Initial version by Attila <s070600>
 */

#include <conio.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <windows.h>
#include <process.h>


// Default serial port settings
#define DEFAULT_SERIAL_PORT "COM1"
#define DEFAULT_BAUD_RATE   115200
#define DEFAULT_PARITY      0

#define MAX_FILES   50
#define NUMBER_OF_CHUNK_ERRORS_TO_SHOW 8
#define SERIAL_READ_RESPONSE_MS 40

#define LC3_CMD_QUERY	"\x1bQ"
#define LC3_CMD_GET_MEM	"\x1bG"
#define LC3_READY_RESPONSE	"Ready."
#define LC3_CMD_UPLOAD	"\x1bU"
#define LC3_CMD_START	"\x1bS"


#define COLOR_LOCAL (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define COLOR_REMOTE (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define COLOR_REMOTE_ESCAPE (FOREGROUND_GREEN)


void receiver_thr_2(void *com);

/************** Synchronized terminal output with colors **************************/

WORD OutputOriginalColors;
HANDLE OutputHandle;
CRITICAL_SECTION OutputCS;

// Set-up environment for output primitives
void output_init() {
	CONSOLE_SCREEN_BUFFER_INFO ConsoleInfo;
	OutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);

	GetConsoleScreenBufferInfo(OutputHandle, &ConsoleInfo);
	OutputOriginalColors = ConsoleInfo.wAttributes;

	// Don't want threads two threads to execute printf at the same time
	InitializeCriticalSectionAndSpinCount(&OutputCS, 0x80000400);
}

// Clean-up environment
void output_cleanup() {
	EnterCriticalSection(&OutputCS);

	SetConsoleTextAttribute(OutputHandle, OutputOriginalColors);

	// will be cleaned-up on process termination
   //DeleteCriticalSection(&OutputCS)
}


int local_message(const char* format, ... ) {
	int ret;
	va_list args;

	EnterCriticalSection(&OutputCS);

	SetConsoleTextAttribute(OutputHandle, COLOR_LOCAL);

	va_start(args, format);
	ret = vprintf(format, args);
	va_end(args);

	LeaveCriticalSection(&OutputCS);

	return ret;
}

void output_received_data(const char* data, int length) {
	DWORD bytesWritten;
	EnterCriticalSection(&OutputCS);

	SetConsoleTextAttribute(OutputHandle, COLOR_REMOTE);

	if (!WriteFile(OutputHandle, data, length, &bytesWritten, NULL)
		 || bytesWritten<length) {
		SetConsoleTextAttribute(OutputHandle, COLOR_LOCAL);
		printf("Write to stdout failed (written %lu of %d, error: %lu)\n",
						  bytesWritten, length, GetLastError());
		return;
	}

	LeaveCriticalSection(&OutputCS);
}

/*******************************************************/

void wait_and_quit() {
	local_message("\npress Enter...");
	getchar();

	output_cleanup();
	exit(1);
}

/*******************************************************/

/*
 * Options
 */
int opt_program_only = 0;
int opt_no_program_check = 0;
int opt_start_address = -1;
int arg_chunk_size = 0;
int arg_dump_start = -1;
int arg_dump_stop = -1;
char * arg_program_files[MAX_FILES];
int arg_files_count;
int arg_retransmit_retry = 2;

char * arg_com_port = DEFAULT_SERIAL_PORT;
int arg_baud_rate = DEFAULT_BAUD_RATE;
int arg_parity = DEFAULT_PARITY;

void usage(const char *progname) {
	local_message("\nUsage:\n"
			"   %s [-p][-n][-c CHUNK_SIZE] [-s BAUD:PARITY:COMPORT] [LC3_OBJ_FILES]\n"
			"   %s [-s BAUD:PARITY:COMPORT] -x LC3_MEMORY_RANGE\n"
					  "  -p: program only. Don't ask LC3 to start uploaded program,\n"
					  "      and don't launch terminal.\n"
					  "  -n: no program check. Don't ask LC3 to send back uploaded program,\n"
					  "  -c: upload files in chunks (given number of bytes) instead of whole file at a time,\n"
					  "  -s: Change default serial settings (3 colon separated fields)\n"
					  "      Valid parity codes: (%d: EVENPARITY, %d: MARKPARITY, %d: NOPARITY, %d: ODDPARITY, %d: SPACEPARITY)\n"
					  "      Default port settings:     -s %d:%d:%s\n"
					  "  -x: Download and hexdump part of LC3 memory\n"
					  "      Valid formats of LC3_MEMORY_RANGE are:\n"
					  "          START_ADDRESS:STOP_ADDRESS (for example: 0x0200:0x02FF)\n"
					  "          START_ADDRESS+WORD_COUNT (for example: 0x0200+0x100)\n"
					  ""
					  "  if LC3_OBJ_FILES are missing programming step is skipped.\n"
					  "  if -p is missing, the last object file specified is started after upload.\n"
					  "\n"
					  ,progname,
					  progname,
					  EVENPARITY, MARKPARITY, NOPARITY, ODDPARITY, SPACEPARITY,
					  DEFAULT_BAUD_RATE, DEFAULT_PARITY, DEFAULT_SERIAL_PORT);

	local_message("\nNote:\n\n"
		"When invoking from Windows file explorer, use following:\n"
		"  * \"Open with\" dialog on the obj file, or\n"
		"  * \"SendTo\" menu on the obj file, or\n"
		"  * Drag&Drop the obj file onto this program.\n");
}

int parse_port(char * arg) {
	char * d1;
	char * d2;

	d1 = strchr(arg, ':');
	if (d1)	d2 = strchr(d1+1, ':');
	if (!d1 || !d2) {
		local_message ("\n   Wrong format of port specification. 'BAUD:PARITY:COMPORT' expected\n");
		return -1;
	}
	arg_com_port = d2+1;
	if (sscanf(arg, "%d:%d:", &arg_baud_rate, &arg_parity) != 2) {
		local_message ("\n   Natural numbers expected in Baud Rate and Parity.\n");
		return -1;
	}
	if (arg_baud_rate <= 0) {
		local_message ("\n   Baud rate should be positive.\n");
		return -1;
	}
	if (arg_parity != EVENPARITY &&
		arg_parity != MARKPARITY &&
		arg_parity != NOPARITY &&
		arg_parity != ODDPARITY &&
		arg_parity != SPACEPARITY) {
		local_message ("\n   Invalid parity code.\n");
		return -1;
	}

	// No error
	return 0;
}



void parse_options(int argc, char **argv){
	char * ext;
	int usage_error = 0;
	int ap = 1;  // argument position

	// Parse options
	while (!usage_error && ap < argc &&
			(argv[ap][0] == '-' || argv[ap][0] == '/')) {
		if (argv[ap][1] == 'p' && !argv[ap][2]) {
			opt_program_only = 1;
			ap++;
		} else if (argv[ap][1] == 'n' && !argv[ap][2]) {
			opt_no_program_check = 1;
			ap++;
		} else if (argv[ap][1] == 'c' && !argv[ap][2]) {
			if (sscanf(argv[ap+1], "%d", &arg_chunk_size) != 1) {
				local_message("Error: option -c expects natural number as CHUNK_SIZE\n\n");
				usage_error = 1;
			}
			ap +=2;
		} else if (argv[ap][1] == 's' && !argv[ap][2]) {
			if (parse_port(argv[ap+1])) {
				usage_error = 1;
			}
			ap +=2;
		} else if (argv[ap][1] == 'x' && !argv[ap][2]) {
			char *arg1 = argv[ap+1];
			char *arg2, *end;

			arg_dump_start = strtol (arg1, &arg2, 0);
			arg_dump_stop = strtol (arg2+1, &end, 0);
			if (*arg2 == '+') {
				// Convert second argument from WORD_COUNT to STOP_ADDRESS
				arg_dump_stop += arg_dump_start - 1;
			}
			if (*end != 0 || (*arg2 != ':' && *arg2 != '+')) {
				local_message("Error: option -x expects pair of natural numbers as LC3_MEMORY_RANGE\n\n");
				usage_error = 1;
			} else if (arg_dump_start < 0 ||
					arg_dump_stop < arg_dump_start ||
					arg_dump_stop > 0xffff) {
				local_message("Error: option -x, wrong address (expected: 0 < START_ADDRESS <= STOP_ADDRESS <= 0xFFFF\n\n");
				usage_error = 1;
			}
			ap +=2;
		} else {
			local_message("Error: unrecognized option %s \n\n", argv[ap]);
			usage_error = 1;
		}
	}

	if (arg_dump_start >= 0 && (arg_chunk_size > 0 ||  opt_program_only || opt_no_program_check || argc > ap)) {
		local_message("Error: \"-x\" can only be used with \"-s\" \n\n");
		usage_error = 1;
	}
	if (opt_program_only && argc <= ap) {
		local_message("Error: \"-p\" without files to program doesn't make sense\n\n");
		usage_error = 1;
	}

	// The rest is object files
	for (; !usage_error && ap < argc; ap++) {
		ext = strrchr(argv[ap], '.');
		if (ext && strcasecmp(ext+1, "obj")==0) {
			arg_program_files[arg_files_count++] = argv[ap];
		} else {
			local_message("Error: Must be invoked with obj file (\"%s\" specified).\n", argv[ap]);
			usage_error = 1;
		}
	}

	if (usage_error) {
		usage(argv[0]);
		wait_and_quit();
	}
}


/************************************************************/

HANDLE open_serial(char *port) {
	COMMTIMEOUTS timeouts={0};
	DCB dcb;
	HANDLE hCom = CreateFile( port,
			GENERIC_READ | GENERIC_WRITE,
			0, // must be opened with exclusive-access
			NULL, // no security attributes
			OPEN_EXISTING, // must use OPEN_EXISTING
			0, // not overlapped I/O
			NULL // hTemplate must be NULL for comm devices
			);
	if (hCom == INVALID_HANDLE_VALUE) {
		local_message("Unable to open serial port \"%s\" (error: %lu)\n", port, GetLastError());
		local_message("Close all programs which are using serial port and try again.\n");
		return NULL;
	}

	// Build on the current configuration, and skip setting the size
	// of the input and output buffers with SetupComm.

	if (!GetCommState(hCom, &dcb)) {
		local_message("GetCommState failed with error %lu.\n", GetLastError());
		return NULL;
	}

	// Fill in DCB: 8 data bits, 1 stop bit, rest according to commandline
	dcb.ByteSize = 8; // data size, xmit, and rcv
	dcb.StopBits = ONESTOPBIT;
	dcb.BaudRate = arg_baud_rate;
	dcb.Parity = arg_parity;

	if (!SetCommState(hCom, &dcb)) {
		local_message("SetCommState failed with error %d.\n", (int)GetLastError());
		return NULL;
	}

   /* Wait on reads */
	timeouts.ReadIntervalTimeout=1;
	timeouts.ReadTotalTimeoutConstant=100;
	timeouts.ReadTotalTimeoutMultiplier=1;
	/*
	timeouts.WriteTotalTimeoutConstant=20;
	timeouts.WriteTotalTimeoutMultiplier=10;
	*/
	if(!SetCommTimeouts(hCom, &timeouts)){
		local_message("SetCommTimeouts failed with error %lu\n", GetLastError());
		return NULL;
	}

	local_message("Serial port %s successfully configured.\n", port);
	return hCom;
}

int send_serial(HANDLE hCom, char * data, int size) {
	DWORD bytesWritten;

	if (!WriteFile(hCom, data, size, &bytesWritten, NULL)
			 || bytesWritten<size) {
			local_message("Write to serial failed (written %lu of %d, error: %lu)\n",
					 bytesWritten, size, GetLastError());
			return -1;
	}

	return bytesWritten;
}

/******* Serial programmer **********/

/*
 * Wait for LC3 to be ready
 */
int wait_lc3_ready(HANDLE hCom) {
	DWORD bytesRead;
	int ready;
	const int nbuff_words = 1024;
	unsigned char buff[nbuff_words*2];
	int respSize = strlen(LC3_READY_RESPONSE);

	do {
		local_message("Quering device...");
		PurgeComm(hCom, PURGE_RXCLEAR);
		if (send_serial(hCom, LC3_CMD_QUERY, 2) < 2) {
			return -1;
		}
		// Wait some time to allow device to respond
		Sleep(100);
		ready = ReadFile(hCom, buff, respSize, &bytesRead, NULL) &&
			      bytesRead==respSize &&
           	   strncmp(buff, LC3_READY_RESPONSE, respSize)==0;
		//		local_message("debug: need:%d[%s], got: %d[%s]\n", respSize, LC3_READY_RESPONSE, bytesRead, buff);
		if (!ready) {
			local_message(" Device not ready.\n"
				"Press PROGRAM push-button (LEFT on the FPGA main board)!\n");
			Sleep(4000);
		}
	} while (!ready);

	return 0;
}

/*
 * Upload part of the file
 */
int send_file_chunk(HANDLE hCom, HANDLE hProg, int file_offset, int lc3_offset, int chunk_size, int progress_done, int progress_whole) {
	const int nbuff_words = 1024;
	unsigned char buff[nbuff_words*2];
	DWORD bytesRead, bytesWritten;
	DWORD remain;	// how many bytes of this chunk remaining to send
	DWORD part;	// how many bytes to send in one go (limited by buffer size and remaining data)

	//// Send Upload command:
	// Header:
	buff[0] = LC3_CMD_UPLOAD[0];
	buff[1] = LC3_CMD_UPLOAD[1];
	// Transmission size: Network order (big-endian) in LC3 words
	buff[2] = (char)(chunk_size/2 / 256);
	buff[3] = (char)(chunk_size/2 % 256);
	// Offset: Network order (big-endian) in LC3 words
	buff[4] = (char)(lc3_offset/2 / 256);
	buff[5] = (char)(lc3_offset/2 % 256);
	if (send_serial(hCom, buff, 6) < 6) {
		return -1;
	}

	SetFilePointer(hProg, file_offset, NULL, FILE_BEGIN);
	SetLastError(ERROR_SUCCESS);
	remain = chunk_size;
	part = min(nbuff_words*2, remain);
	while (remain &&
			ReadFile(hProg, buff, part, &bytesRead, NULL)
			&& bytesRead!=0) {
		if (bytesRead < part) {
			local_message("Warning: ReadFile() returned only some of requested data\n");
		}
		if ((bytesWritten=send_serial(hCom, buff, bytesRead)) < bytesRead) {
			// send_serial should have send all requested data
			return -1;
		} else {
			remain -= bytesWritten;
			local_message("\r%3d%% complete     --  %5lu of %5lu bytes send              ",
				       (int)((progress_done+chunk_size-remain)*100/progress_whole), (progress_done+chunk_size-remain), progress_whole);
		}
		part = min(nbuff_words*2, remain);
	}

	return (remain) ? -1 : 0;
}

/*
 * Check for transmission errors
 */
int check_file_chunk(HANDLE hCom, HANDLE hProg, int file_offset, int lc3_offset, int chunk_size, int *pErr_cnt, int progress_done, int progress_whole) {
	const int nbuff_words = 1024;
	unsigned char buff[nbuff_words*2];
	DWORD bytesRead;
	DWORD remain;	// how many bytes of this chunk remaining to send
	DWORD part;	// how many bytes to send in one go (limited by buffer size and remaining data)

	// Wait some time to allow device to respond
	Sleep(200);
	PurgeComm(hCom, PURGE_RXCLEAR);

	//// Send Memory Download command:
	// Header:
	buff[0] = LC3_CMD_GET_MEM[0];
	buff[1] = LC3_CMD_GET_MEM[1];
	// Transmission size: Network order (big-endian) in LC3 words
	buff[2] = (char)(chunk_size/2 / 256);
	buff[3] = (char)(chunk_size/2 % 256);
	// Offset: Network order (big-endian) in LC3 words
	buff[4] = (char)(lc3_offset/2 / 256);
	buff[5] = (char)(lc3_offset/2 % 256);
	if (send_serial(hCom, buff, 6) < 6) {
		return -1;
	}

	*pErr_cnt = 0;
	{
		unsigned char lc3_buff[nbuff_words*2];
		DWORD lc3_bytesRead;
		int i;

		// Wait some time to allow device to respond
		SetFilePointer(hProg, file_offset, NULL, FILE_BEGIN); // Set file pointer at the start of the program (omit offset)
		SetLastError(ERROR_SUCCESS);
		remain = chunk_size;
		part = min(nbuff_words*2, remain);
		while (remain &&
				ReadFile(hProg, buff, part, &bytesRead, NULL)
				&& bytesRead!=0) {
			if (bytesRead < part) {
				local_message("Warning: ReadFile() returned only some of requested data\n");
			}
			if (!ReadFile(hCom, lc3_buff, bytesRead, &lc3_bytesRead, NULL)
					|| lc3_bytesRead < bytesRead){
				local_message("Read from serial failed (received %lu of %d, error: %lu)\n",
						lc3_bytesRead, bytesRead, GetLastError());
				return -1;
			}
			for (i=0; i < bytesRead; i++) {
				if (buff[i] != lc3_buff[i]) {
					(*pErr_cnt)++;
					if (*pErr_cnt < NUMBER_OF_CHUNK_ERRORS_TO_SHOW) {
						local_message("\r Error at byte %d (send:0x%02x, received:0x%02x)                 \n",
								file_offset+(chunk_size-remain)+i, buff[i], lc3_buff[i]);
					} else if (*pErr_cnt == NUMBER_OF_CHUNK_ERRORS_TO_SHOW) {
						local_message("\r Error at byte %d (send:0x%02x, received:0x%02x) (next errors will not be reported)\n",
								file_offset+(chunk_size-remain)+i, buff[i], lc3_buff[i]);
					}

				}
			}

			remain -= bytesRead;
			local_message("\r%3d%% complete     --  %5lu of %5lu bytes verified      ",
				       (int)((progress_done+chunk_size-remain)*100/progress_whole), (progress_done+chunk_size-remain), progress_whole);
			part = min(nbuff_words*2, remain);
		}
	}

	return 0;
}

int program_device(HANDLE hCom, const char *program_file) {
	int err_cnt, chunk_err_cnt, try, failed_chunks;
	const int nbuff_words = 1024;
	unsigned char buff[nbuff_words*2];
	unsigned short progStart;
	DWORD progSize;
	HANDLE hProg;
	DWORD bytesRead;
	DWORD chunk_size, file_pos, lc3_pos, part, remain;

	hProg = CreateFile(program_file, GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
	if (hProg == INVALID_HANDLE_VALUE) {
		local_message("Unable to open \"%s\" for reading (error: %lu)\n", program_file, GetLastError());
		wait_and_quit();
	}

	progSize = GetFileSize(hProg, NULL);
	progSize = progSize-2; // lc3_offset from file is counted separately

	if (progSize % 2 || progSize > 0x10000) {
		local_message("Error: Obj file's size has to be even and fit into LC3 memory\n");
		CloseHandle(hProg);
		return -1;
	}

	if (!ReadFile(hProg, buff, 4, &bytesRead, NULL)
          || bytesRead<4) {
		local_message("Error: unable to read \"%s\"\n", program_file);
		CloseHandle(hProg);
		return -1;
	}

	// This is obj file, need to calculate transmission size
	progStart = (buff[0] << 8) + buff[1];
	opt_start_address = progStart;

	if (wait_lc3_ready(hCom) < 0) {
		CloseHandle(hProg);
		return -1;
	}

	local_message("\nUploading \"%s\":\n\t%u words starting from x%04x\n",
		      program_file, progSize/2, progStart);

	remain = progSize;
	err_cnt = failed_chunks = 0;
	chunk_size = (arg_chunk_size) ? arg_chunk_size : remain;
	file_pos = 2;
	lc3_pos = progStart*2;  // scale LC3 offset from words to bytes
	while (remain) {
		part = min(remain, chunk_size);

		try = 0;
	Retry:
		if (send_file_chunk(hCom, hProg, file_pos, lc3_pos, part, progSize-remain, progSize) < 0) {
			CloseHandle(hProg);
			return -1;
		}
		local_message("\r%3d%% complete     --  %5lu of %5lu bytes send              ",
				   (int)((progSize-remain+part)*100/progSize), progSize-remain+part, progSize);
		if (!opt_no_program_check) {
			chunk_err_cnt = 0;
			if (check_file_chunk(hCom, hProg, file_pos, lc3_pos, part, &chunk_err_cnt, progSize-remain, progSize) < 0) {
				CloseHandle(hProg);
				return -1;
			} else if (chunk_err_cnt != 0) {
				if (try < arg_retransmit_retry) {
					try++;
					local_message("\r%d errors discovered in chunk. Trying to retransmit (%d time)\n", chunk_err_cnt, try);
					goto Retry;
				} else {
					local_message("\r%d errors discovered in chunk. %d retransmissions failed.\n", chunk_err_cnt, try);
					err_cnt += chunk_err_cnt;
					failed_chunks++;
				}
			} else {
				local_message( (try
								? "\rRetransmission successful.                                      \n"
								: "\r%3d%% complete     --  %5lu of %5lu bytes verified              "),
						(int)((progSize-remain+part)*100/progSize), (progSize-remain+part), progSize);
			}
		}

		remain -= part;
		file_pos += part;
		lc3_pos += part;

	}
	local_message("\n");


	if (!opt_no_program_check) {
		if (err_cnt == 0) {
			local_message("\n\n No errors found in final transmission of \"%s\" \n\n", program_file);
		} else {
			local_message("\nWARNING:"
						  "\nWARNING: %d errors in %d chunks NOT-FIXED during retransmission of \"%s\""
						  "\nWARNING:\n", err_cnt, failed_chunks, program_file);
			//local_message("ERROR: %d errors discovered during verification of \"%s\"\n", err_cnt, program_file);
			//CloseHandle(hProg);
			//return -1;
		}
	}
	if (GetLastError() != ERROR_SUCCESS) {
		local_message("Read of \"%s\" failed with error %lu\n", program_file, GetLastError());
		CloseHandle(hProg);
		return -1;
	}

	CloseHandle(hProg);
	return 0;
}

int dump_lc3_memory(HANDLE hCom, int lc3_word_start, int lc3_word_stop) {
	const int nbuff_words = 1024;
	unsigned char buff[nbuff_words*2];
	int w;
	DWORD bytesRead;
	DWORD words_remain;	// how many words left to receive
	DWORD words_part;	// how many words to send in one go (limited by buffer size and remaining data)
	DWORD words_done;// how many words were allready received from the start of requested address

	words_remain = lc3_word_stop - lc3_word_start + 1;

	// Wait some time to allow device to respond
	Sleep(200);
	PurgeComm(hCom, PURGE_RXCLEAR);

	//// Send Memory Download command:
	// Header:
	buff[0] = LC3_CMD_GET_MEM[0];
	buff[1] = LC3_CMD_GET_MEM[1];
	// Transmission size: Network order (big-endian) in LC3 words
	buff[2] = (char)(words_remain / 256);
	buff[3] = (char)(words_remain % 256);
	// Offset: Network order (big-endian) in LC3 words
	buff[4] = (char)(lc3_word_start / 256);
	buff[5] = (char)(lc3_word_start % 256);
	if (send_serial(hCom, buff, 6) < 6) {
		return -1;
	}

	words_done = 0;

	// Wait some time to allow device to respond
	Sleep(200);
	SetLastError(ERROR_SUCCESS);
	words_part = min(nbuff_words, words_remain);
	while (words_remain) {
		if (!ReadFile(hCom, buff, 2*words_part, &bytesRead, NULL)
				|| bytesRead < 2*words_part){
			local_message("Read from serial failed (received %lu of %d, error: %lu)\n",
					bytesRead, 2*words_part, GetLastError());
			return -1;
		}
		for (w=0; w < bytesRead/2; w++) {
			if ((words_done+w) % 8 == 0) {
				printf("\nx%04X:", lc3_word_start+words_done+w);
			}
			if ((words_done+w) % 4 == 0) {
				printf(" ");
			}
			printf(" x%02X%02X", buff[w*2], buff[w*2+1]);
		}

		words_remain -= bytesRead/2;
		words_done += bytesRead/2;
		words_part = min(nbuff_words, words_remain);
	}

	return 0;
}


/******* Serial console **********/

void receiver_thr(void *com) {
	HANDLE hCom = (HANDLE) com;
	char buff[256];
	DWORD bytesRead;

	/* Used by WaitCommEvent, which is disabled
	 *    DWORD dwEvtMask;
	 *    BOOL fSuccess;
	 *    // Set the event mask.
	 *    if (!SetCommMask(hCom, EV_RXCHAR)) {
	 *    {
	 *    	local_message("SetCommMask failed with error %d.\n", GetLastError());
	 *    	wait_and_quit();
	 *    }
	 */
	local_message("Console started.\n");

	while (1) {
		/*  This code doesn't work, because it locks the serial (no write would be possible).
		 *  So we use poling (see code below)
		 *	if (!WaitCommEvent(hCom, &dwEvtMask, NULL) ||
		 *		!(dwEvtMask & EV_RXCHAR)
		 *	   )
		 *	{
		 *		local_message("WaitCommEvent failed (error: %d, mask: 0x%X).\n", GetLastError(), dwEvtMask);
		 *		wait_and_quit();
		 *	}
		 */

		bytesRead = 0;
		ReadFile(hCom, buff, sizeof(buff), &bytesRead, NULL);
		if (bytesRead > 0) {
			output_received_data(buff, bytesRead);
		} else {
			// Give CPU to other task.
			Sleep(SERIAL_READ_RESPONSE_MS);
		}
	}
}
void receiver_thr_2(void *com) {
	HANDLE hCom = (HANDLE) com;
	char buff[256];
	DWORD bytesRead;

	/* Used by WaitCommEvent, which is disabled
	 *    DWORD dwEvtMask;
	 *    BOOL fSuccess;
	 *    // Set the event mask.
	 *    if (!SetCommMask(hCom, EV_RXCHAR)) {
	 *    {
	 *    	local_message("SetCommMask failed with error %d.\n", GetLastError());
	 *    	wait_and_quit();
	 *    }
	 */
	local_message("Console started.\n");

	while (1) {
		/*  This code doesn't work, because it locks the serial (no write would be possible).
		 *  So we use poling (see code below)
		 *	if (!WaitCommEvent(hCom, &dwEvtMask, NULL) ||
		 *		!(dwEvtMask & EV_RXCHAR)
		 *	   )
		 *	{
		 *		local_message("WaitCommEvent failed (error: %d, mask: 0x%X).\n", GetLastError(), dwEvtMask);
		 *		wait_and_quit();
		 *	}
		 */

		bytesRead = 0;
		ReadFile(hCom, buff, sizeof(buff), &bytesRead, NULL);
		if (bytesRead > 0) {
			int i;
			for (i=0; i<bytesRead; i++)
				printf("%02x ", (unsigned char)buff[i]);
			printf("\n");
		} else {
			// Give CPU to other task.
			Sleep(SERIAL_READ_RESPONSE_MS);
		}
	}
}

int start_terminal_mode(HANDLE hCom) {
	unsigned int thread_id = 0;
	unsigned char databyte = 0;
	COMMTIMEOUTS timeouts={0};


   /* Non blocking reads */
	timeouts.ReadIntervalTimeout=MAXDWORD;
	timeouts.ReadTotalTimeoutConstant=0;
	timeouts.ReadTotalTimeoutMultiplier=0;
	/*
	timeouts.WriteTotalTimeoutConstant=20;
	timeouts.WriteTotalTimeoutMultiplier=10;
	*/
	if(!SetCommTimeouts(hCom, &timeouts)){
		local_message("SetCommTimeouts failed with error %lu\n", GetLastError());
		return -1;
	}

	if ((thread_id = _beginthread(receiver_thr,4096,(void *)hCom)) < 0)
	{
		local_message("Unable to create thread (ret: %d, errno: %d)\n", thread_id, errno);
		return -1;
	}

	while (1) {
		databyte = getch();
		/* It's probably fine if we send special key strokes too.
		 * if (databyte==0 || databyte==0xe0) {
		 * 	// special key hit
		 * 	local_message("<special key ignorred>");
		 * 	getch();
		 * 	continue;
		 * }
		 */
		// some translations are needed
		if (databyte == 13) {
			databyte = '\n';
		}
		send_serial(hCom, &databyte, 1);
	}

	return 0;
}
/************************************************************/




int main(int argc, char *argv[])
{
	HANDLE hCom;
	int i;

	output_init();

	parse_options(argc, argv);

	hCom = open_serial(arg_com_port);
	if (!hCom) {
		wait_and_quit();
	}

	if (arg_dump_start >= 0) {
		local_message("Will try to download the content of LC3 memory x%04x:x%04x.\n", arg_dump_start, arg_dump_stop);

		if (wait_lc3_ready(hCom) == 0) {
			dump_lc3_memory(hCom, arg_dump_start, arg_dump_stop);
		}
		CloseHandle(hCom);
		return 0;
	} else if (arg_files_count == 0) {
		local_message("Upload operation skipped (no obj file specified).\n");
	} else {
		for (i=0; i < arg_files_count; i++) {
			if (program_device(hCom, arg_program_files[i]) < 0) {
				wait_and_quit();
			}
			if (i+1 < arg_files_count) {
				// Give device some slack, before programming next chunk
				Sleep(200);
			}
		}

		if (!opt_program_only) {
			char buff[] = { (char)((unsigned)opt_start_address / 256),
								 (char)((unsigned)opt_start_address % 256)};
			Sleep(100);
			PurgeComm(hCom, PURGE_RXCLEAR);
			local_message("Starting uploaded program (at: x%04X).", opt_start_address);
			if (send_serial(hCom, LC3_CMD_START, 2) < 2) {
				local_message("Starting Failed.\n");
			}
			if (send_serial(hCom, buff, 2) < 2) {
				local_message("Starting Failed.\n");
			}
		}
	}

	if (!opt_program_only) {
		if (start_terminal_mode(hCom) < 0) {
			wait_and_quit();
		}
	}

	// If terminal mode is started this point is never reached
	// Files are closed automatically by OS then program is terminated by the user.
	CloseHandle(hCom);

	return 0;
}
