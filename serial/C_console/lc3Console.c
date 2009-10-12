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


#define SERIAL_READ_RESPONSE_MS 40


int color_printf(WORD color, const char* format, ... ) {
	va_list args;
 	WORD originalColors;
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

	CONSOLE_SCREEN_BUFFER_INFO ConsoleInfo;
	GetConsoleScreenBufferInfo(hStdout, &ConsoleInfo);
	originalColors = ConsoleInfo.wAttributes;

	SetConsoleTextAttribute(hStdout, color | FOREGROUND_INTENSITY);

	va_start(args, format);
	vprintf(format, args);
	va_end(args);

	SetConsoleTextAttribute(hStdout, originalColors);
	return 0;
}

void wait_and_quit() {
	printf("\npress Enter...");
	getchar();

	exit(1);
}


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
		printf("Unable to open serial port (error: %lu)\n", GetLastError());
		return NULL;
	}

	// Build on the current configuration, and skip setting the size
	// of the input and output buffers with SetupComm.

	if (!GetCommState(hCom, &dcb)) {
		printf("GetCommState failed with error %lu.\n", GetLastError());
		return NULL;
	}

	// Fill in DCB: 115,200 bps, 8 data bits, no parity, and 1 stop bit.
	dcb.BaudRate = CBR_115200; // set the baud rate
	dcb.ByteSize = 8; // data size, xmit, and rcv
	dcb.Parity = NOPARITY; // no parity bit
	dcb.StopBits = ONESTOPBIT; // one stop bit

	if (!SetCommState(hCom, &dcb)) {
		printf("SetCommState failed with error %d.\n", (int)GetLastError());
		return NULL;
	}

	printf("Serial port %s successfully reconfigured.\n", port);
	return hCom;
}

/******* Serial programmer **********/

int program_device(HANDLE hCom, HANDLE hProg, int is_ser_file) {
	const int nbuff_words = 1024;
	char buff[nbuff_words*2];
	DWORD bytesRead, bytesWritten;
	DWORD total = 0;
	DWORD progSize = GetFileSize(hProg, NULL);

	if (progSize % 2 || progSize > 0x10000) {
		printf("Error: Obj file's size has to be even\n");
		return -1;
	}

	printf("Uploading program...\n");

	if (!is_ser_file) {
		// This is obj file, need to send transmission size before the data 
		// This has to be send in Network order (big-endian)
		unsigned short words = progSize/2 - 1;
	   buff[0] = (char)(words / 256);
		buff[1] = (char)(words % 256);
		if (!WriteFile(hCom, buff, 2, &bytesWritten, NULL)
			 || bytesWritten<2) {
			printf("Write to serial failed (written %lu of %d, error: %lu)\n",
					 bytesWritten, 2, GetLastError());
			return -1;
		}
	}

	SetLastError(ERROR_SUCCESS);
	while (ReadFile(hProg, buff, nbuff_words*2, &bytesRead, NULL)
          && bytesRead!=0) {
		if (!WriteFile(hCom, buff, bytesRead, &bytesWritten, NULL) ||
			 bytesRead != bytesWritten) {
			printf("Write to serial failed (written %lu of %lu, error: %lu)\n",
					 bytesWritten, bytesRead, GetLastError());
			SetLastError(ERROR_SUCCESS);
		} else {
			total += bytesWritten;
			fprintf(stderr, "\r%3d%% complete (%5lu of %5lu bytes send)", (int)(total*100/progSize), total, progSize);
		}
	}

	if (GetLastError() != ERROR_SUCCESS) {
		printf("\nRead of obj file failed with error %lu\n", GetLastError());
		return -1;
	}
	fprintf(stderr, "\n");

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
	 *    	printf("SetCommMask failed with error %d.\n", GetLastError());
	 *    	wait_and_quit();
	 *    }
    */
   printf("Console started.");

	while (1) {
     /*  This code doesn't work, because it locks the serial (no write would be possible).
      *  So we use poling (see code below)
	   *	if (!WaitCommEvent(hCom, &dwEvtMask, NULL) ||
	   *		!(dwEvtMask & EV_RXCHAR)
	   *	   )
	   *	{
	   *		printf("WaitCommEvent failed (error: %d, mask: 0x%X).\n", GetLastError(), dwEvtMask);
	   *		wait_and_quit();
	   *	}
      */

		bytesRead = 0;
		ReadFile(hCom, buff, sizeof(buff), &bytesRead, NULL);
		if (bytesRead > 0) {
			// TODO: escape non-printables
			fwrite(buff, bytesRead, 1, stdout);
		} else {
        // Give CPU to other task.
		    Sleep(SERIAL_READ_RESPONSE_MS);
		    }
	}
}

int start_console_mode(HANDLE hCom) {
	unsigned int thread_id = 0;
	unsigned char databyte = 0;
	static DWORD bytesWritten;
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
		printf("SetCommTimeouts failed with error %lu\n", GetLastError());
		return -1;
	}

	if ((thread_id = _beginthread(receiver_thr,4096,(void *)hCom)) < 0)
	{
		printf("Unable to create thread (ret: %d, errno: %d)\n", thread_id, errno);
		return -1;
	}
   
	while (1) {
		databyte = getch();
		/* It's probably fine if we send special key strokes too.
		 * if (databyte==0 || databyte==0xe0) {
		 * 	// special key hit
		 * 	printf("<special key ignorred>");
		 * 	getch();
		 * 	continue;
		 * }
       */
      // some translations are needed
      if (databyte == 13) {
        databyte = '\n';
      }
		if (!WriteFile(hCom, &databyte, 1, &bytesWritten, NULL) ) {
			printf("Write to serial failed (error: %lu)\n",
				GetLastError());
		}
	}

	return 0;
}
/************************************************************/

/*
 * Options
 */
int opt_is_ser_file;
char * arg_program_file;
char * arg_com_port = "COM1";

void parse_options(int argc, char *argv[]){
   char * ext;
   
	if (argc <= 1) {
		printf("Usage: %s LC3_OBJ_FILE\n", argv[0]);
		printf("\nNote:\n\n"
			"When invoking from Windows file explorer, use following:\n"
			"  * \"Open with\" dialog on the obj file, or\n"
			"  * Drag&Drop the obj file onto this program.\n");
		wait_and_quit();
	} else {
		arg_program_file = argv[1];
		ext = strrchr(arg_program_file, '.');
		if (ext && strcasecmp(ext+1, "ser")==0) {
			opt_is_ser_file = 1;
		} else if (ext && strcasecmp(ext+1, "obj")==0) {
			opt_is_ser_file = 0;
		} else {
			printf("Error: Must be invoked with obj file.\n");
			wait_and_quit();
		}
	}
}


/************************************************************/



int main(int argc, char *argv[])
{
	HANDLE hCom, hProg;


	parse_options(argc, argv);

	hProg = CreateFile(arg_program_file, GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
	if (hProg == INVALID_HANDLE_VALUE) {
		printf("Unable to open \"%s\" for reading (error: %lu)\n", arg_program_file, GetLastError());
		wait_and_quit();
	}

	hCom = open_serial(arg_com_port);
	if (!hCom) {
		wait_and_quit();
	}

	if (program_device(hCom, hProg, opt_is_ser_file) < 0) {
		wait_and_quit();
	}
	CloseHandle(hProg);


	if (start_console_mode(hCom) < 0) {
		wait_and_quit();
	}
	// this point is never reached
	// Files are closed automatically by OS then program is terminated by the user.
	//
	// CloseHandle(hCom);

	return 0;
}
