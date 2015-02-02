#include <windows.h>
#include <string.h>
#include <stdio.h>

DWORD str2baudrate(char *baudrate) {
    if (!strcmp("115200", baudrate))
        return CBR_115200;
    if (!strcmp("57600", baudrate))
        return CBR_57600;
    if (!strcmp("38400", baudrate))
        return CBR_38400;
    if (!strcmp("19200", baudrate))
        return CBR_19200;
    if (!strcmp("9600", baudrate))
        return CBR_9600;
    if (!strcmp("4800", baudrate))
        return CBR_4800;
    if (!strcmp("2400", baudrate))
        return CBR_2400;
    if (!strcmp("1200", baudrate))
        return CBR_1200;

    return 0xFFFF;
}


HANDLE init_serial(char *port, char *baudrate) {

	DWORD br = str2baudrate(baudrate);
    if (br == 0xFFFF) {
		fprintf(stderr, "Invalid baudrate\n");
        return INVALID_HANDLE_VALUE;
	}

			
	HANDLE handle = CreateFile(port, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, NULL);
	if ( handle == INVALID_HANDLE_VALUE ) {
		fprintf(stderr, "Error opening serial device\n");
        return INVALID_HANDLE_VALUE;
	}
	
	DCB dcb;

	GetCommState(handle,&dcb);
	dcb.BaudRate = br;
	dcb.ByteSize = 7;
	dcb.Parity = NOPARITY;
	dcb.StopBits = TWOSTOPBITS;

	if ( !SetCommState(handle,&dcb) ) {
		fprintf(stderr, "Error setting CommState\n");
        return INVALID_HANDLE_VALUE;
	}

	if ( !SetupComm(handle,4096,4096) ) {
		fprintf(stderr, "Error setting up serial device\n");
        return INVALID_HANDLE_VALUE;
	}

	COMMTIMEOUTS CommTimeouts;

	CommTimeouts.ReadIntervalTimeout = MAXDWORD;
	CommTimeouts.ReadTotalTimeoutConstant = 2;
	CommTimeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
	CommTimeouts.WriteTotalTimeoutConstant = 0;
	CommTimeouts.WriteTotalTimeoutMultiplier = 0;

	if ( !SetCommTimeouts(handle,&CommTimeouts) ) {
		fprintf(stderr, "Error setting comm timeouts\n");
        return INVALID_HANDLE_VALUE;
	}
	
	return handle;
}

int send_str(HANDLE handle, char *buf) {
	DWORD dwWritten = 0;
    int len = strlen(buf);

	if (!WriteFile(handle, buf, len, &dwWritten, NULL) || dwWritten < len) {
        fprintf(stderr, "Error writing to serial device\n");
		return 0;
    }
    return 1;
}

int read_char(HANDLE handle, char *ch) {
	DWORD nBytesRead;
    if ( ReadFile(handle,ch,1,&nBytesRead,NULL) ) {
        return nBytesRead;
    }
    return 0;
}

