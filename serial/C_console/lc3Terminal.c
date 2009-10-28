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
#include <stdarg.h>
#include <windows.h>
#include <process.h>


#define SERIAL_PORT "COM1"
#define MAX_FILES   50

#define SERIAL_READ_RESPONSE_MS 40

#define LC3_CMD_QUERY	"\x1bQ"
#define LC3_READY_RESPONSE	"Ready."
#define LC3_CMD_UPLOAD	"\x1bU"
#define LC3_CMD_START	"\x1bS"


#define COLOR_LOCAL (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define COLOR_REMOTE (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define COLOR_REMOTE_ESCAPE (FOREGROUND_GREEN)



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
int opt_skip_upload = 0;
int opt_program_only = 0;
int opt_start_address = -1;
#ifdef USE_SER_FILES
int opt_is_ser_file;
#endif
char * arg_program_files[MAX_FILES];
int arg_files_count;
char * arg_com_port = SERIAL_PORT;

void usage(const char *progname) {
	local_message("\nUsage: %s [-p] [LC3_OBJ_FILES]\n"
					  "  -p: program only. Don't ask LC3 to start uploaded program,\n"
					  "      and don't launch terminal.\n"
					  "  if LC3_OBJ_FILES are missing programming step is skipped.\n"
					  "  if -p is missing, the last object file specified is started after upload.\n"
					  "\n"
					  , progname);

	local_message("\nNote:\n\n"
		"When invoking from Windows file explorer, use following:\n"
		"  * \"Open with\" dialog on the obj file, or\n"
		"  * \"SendTo\" menu on the obj file, or\n"
		"  * Drag&Drop the obj file onto this program.\n");
}

void parse_options(int argc, char **argv){
   char * ext;
	int usage_error = 0;
	int arg_pos = 1;

	// Parse options
	if (argc <= arg_pos) {
		// No options neither arguments
		opt_skip_upload = 1;
	} else if (argv[1][0] == '-' || argv[1][0] == '/') {
		if (argv[1][1] == 'p') {
			opt_program_only = 1;
			arg_pos++;
		} else {
			local_message("Error: unrecognized option\n\n");
			usage_error = 1;
		}
	}
	
	if (opt_program_only && argc <= arg_pos) {
		local_message("Error: \"-p\" without files to program doesn't make sense\n\n");
		usage_error = 1;
	}

	// Parse arguments
	for (; !usage_error && arg_pos < argc; arg_pos++) {
		ext = strrchr(argv[arg_pos], '.');
#ifdef USE_SER_FILES
		if (ext && strcasecmp(ext+1, "ser")==0) {
			opt_is_ser_file = 1;
		} else
#endif
		if (ext && strcasecmp(ext+1, "obj")==0) {
			arg_program_files[arg_files_count++] = argv[arg_pos];
#ifdef USE_SER_FILES
			opt_is_ser_file = 0;
#endif
		} else {
			local_message("Error: Must be invoked with obj file (\"%s\" specified).\n", argv[arg_pos]);
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
		local_message("Unable to open %s serial port (error: %lu)\n", port, GetLastError());
		local_message("Close all programs which are using serial port and try again.\n");
		return NULL;
	}

	// Build on the current configuration, and skip setting the size
	// of the input and output buffers with SetupComm.

	if (!GetCommState(hCom, &dcb)) {
		local_message("GetCommState failed with error %lu.\n", GetLastError());
		return NULL;
	}

	// Fill in DCB: 115,200 bps, 8 data bits, no parity, and 1 stop bit.
	dcb.BaudRate = CBR_115200; // set the baud rate
	dcb.ByteSize = 8; // data size, xmit, and rcv
	dcb.Parity = NOPARITY; // no parity bit
	dcb.StopBits = ONESTOPBIT; // one stop bit

	if (!SetCommState(hCom, &dcb)) {
		local_message("SetCommState failed with error %d.\n", (int)GetLastError());
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

#ifdef USE_SER_FILES
int program_device(HANDLE hCom, const char *program_file, int is_ser_file) {
#else
int program_device(HANDLE hCom, const char *program_file) {
#endif
	int ready;
	const int nbuff_words = 1024;
	unsigned char buff[nbuff_words*2];
	unsigned short progStart, progWords;
	DWORD progSize;
	HANDLE hProg;
	DWORD bytesRead, bytesWritten;
	DWORD total;

	hProg = CreateFile(program_file, GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
	if (hProg == INVALID_HANDLE_VALUE) {
		local_message("Unable to open \"%s\" for reading (error: %lu)\n", program_file, GetLastError());
		wait_and_quit();
	}

	progSize = GetFileSize(hProg, NULL);

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
#ifdef USE_SER_FILES
	if (is_ser_file) {
		// This is ser file, transmission size is first word
		// The data is in Network order (big-endian)
		progWords = (buff[0] << 8) + buff[1];
		progStart = (buff[2] << 8) + buff[3];
	} else
#endif
	{
		// This is obj file, need to calulate transmission size
		progWords = progSize/2 - 1;
		progStart = (buff[0] << 8) + buff[1];
	}
	SetFilePointer(hProg, 0, NULL, FILE_BEGIN);
	opt_start_address = progStart;


	// Query device
	do {
		int respSize = strlen(LC3_READY_RESPONSE);

		local_message("Quering device...");
		PurgeComm(hCom, PURGE_RXCLEAR);
		if (send_serial(hCom, LC3_CMD_QUERY, 2) < 2) {
			CloseHandle(hProg);
			return -1;
		}
		// Wait some time to allow device to respond
		Sleep(100);
		ReadFile(hCom, buff, respSize, &bytesRead, NULL);
		ready = strncmp(buff, LC3_READY_RESPONSE, respSize)==0;
		if (!ready) {
			local_message(" Device not ready.\n"
				"Press PROGRAM push-button (LEFT on the FPGA main board)!\n");
			Sleep(4000);
		}
	} while (!ready);

	local_message("\nUploading \"%s\":\n\t%u words starting from x%04x\n",
		      program_file, progWords, progStart);
	if (send_serial(hCom, LC3_CMD_UPLOAD, 2) < 2) {
		CloseHandle(hProg);
		return -1;
	}

#ifdef USE_SER_FILES
	if (!is_ser_file) {
#else
	if (1) {
#endif
		// This is obj file, need to send transmission size before the data
		// This has to be send in Network order (big-endian)
		buff[0] = (char)(progWords / 256);
		buff[1] = (char)(progWords % 256);
		if (send_serial(hCom, buff, 2) < 2) {
			CloseHandle(hProg);
			return -1;
		}
	}

	total = 0;
	SetLastError(ERROR_SUCCESS);
	while (ReadFile(hProg, buff, nbuff_words*2, &bytesRead, NULL)
          && bytesRead!=0) {
		if ((bytesWritten=send_serial(hCom, buff, bytesRead)) < bytesRead) {
			SetLastError(ERROR_SUCCESS);
		} else {
			total += bytesWritten;
			local_message("\r%3d%% complete     --  %5lu of %5lu bytes send",
				       (int)(total*100/progSize), total, progSize);
		}
	}
	local_message("\n");

	if (GetLastError() != ERROR_SUCCESS) {
		local_message("Read of \"%s\" failed with error %lu\n", program_file, GetLastError());
		CloseHandle(hProg);
		return -1;
	}

	CloseHandle(hProg);
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

	if (opt_skip_upload) {
		local_message("Upload operation skipped (no obj file specified).\n");
	} else {
		for (i=0; i < arg_files_count; i++) {
#ifdef USE_SER_FILES
			if (program_device(hCom, arg_program_files[i], opt_is_ser_file) < 0) {
#else
			if (program_device(hCom, arg_program_files[i]) < 0) {
#endif
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
