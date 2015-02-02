#ifndef __SERIAL_INCLUDED
#define __SERIAL_INCLUDED

HANDLE init_serial(char *port, char *baudrate);
int send_str(HANDLE handle, char *buf);
int read_char(HANDLE handle, char *ch);

#endif /* __SERIAL_INCLUDED */
