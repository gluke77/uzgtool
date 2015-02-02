#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include "serial.h"
#include "modbus_msg.h"

modbus_msg_s* recv_modbus_msg(HANDLE serial, modbus_msg_s *msg);
int send_modbus_msg(HANDLE serial, modbus_msg_s msg);
modbus_msg_s* send_recv(HANDLE serial, modbus_msg_s *msg);
int range_scan(HANDLE serial, int device_id, int lo, int hi, int step);


int main(int argc, char * argv[])
{
    if (argc != 7) {
        fprintf(stderr, "Usage: usbtool <port> <baudrate> <device_id> <lo_freq> <hi_freq> <step>\n");
        return -1;
    }

    HANDLE serial = init_serial(argv[1], argv[2]);
    if (serial == INVALID_HANDLE_VALUE) {
        return -1;
    }

    range_scan(serial, atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]));

	if ( !CloseHandle(serial) )
		fprintf(stderr, "Error closing serial port\n");

	return 0;
}

modbus_msg_s* recv_modbus_msg(HANDLE serial, modbus_msg_s *msg) {
    char buf[MODBUS_MAX_MSG_LENGTH];
    char ch;

    while (!msg_ready(buf)) {
        if (read_char(serial, &ch))
            add_char(buf, ch);
    }

    reset_msg(msg);
    return str2msg(buf, msg, MODBUS_RESPONSE);

}

modbus_msg_s* send_recv(HANDLE serial, modbus_msg_s *msg) {

    if (send_modbus_msg(serial, *msg))
        return recv_modbus_msg(serial, msg);

    return NULL;
}

int send_modbus_msg(HANDLE serial, modbus_msg_s msg) {
    char buf[MODBUS_MAX_MSG_LENGTH];

    if (!msg2str(msg, buf)) {
        fprintf(stderr, "Error converting modbus message to string\n");
        return 0;
    }

    return send_str(serial, buf);
}

int range_scan(HANDLE serial, int device_id, int lo, int hi, int step) {
    modbus_msg_s msg;
    
    // get freq bounds
    reset_msg(&msg);
    msg.type = MODBUS_COMMAND;
    msg.command = MODBUS_READ;
    msg.address = 1;
    msg.device_id = device_id;
    msg.data[0] = 1;

    if (!send_recv(serial, &msg) || msg.type == MODBUS_ERROR) {
        fprintf(stderr, "Error communicating with device\n");
        return 0;
    }

    hi = msg.data[0] > hi ? hi : msg.data[0];
    lo = msg.data[1] < lo ? lo : msg.data[1];

    // run uzg, keep off
    reset_msg(&msg);
    msg.type = MODBUS_COMMAND;
    msg.command = MODBUS_WRITE;
    msg.address = 0;
    msg.device_id = device_id;
    msg.data[0] = 0x0002 | 0x0010;

    if (!send_recv(serial, &msg) || msg.type == MODBUS_ERROR) {
        fprintf(stderr, "Error communicating with device\n");
        return 0;
    }

    for (int freq = lo; freq <= hi; freq += step) {

        // set freq
        reset_msg(&msg);
        msg.type = MODBUS_COMMAND;
        msg.command = MODBUS_WRITE;
        msg.address = 1;
        msg.device_id = device_id;
        msg.data[0] = freq;

        if (!send_recv(serial, &msg) || msg.type == MODBUS_ERROR) {
            fprintf(stderr, "Error communicating with device\n");
            return 0;
        }

        Sleep(100);

        // get status
        reset_msg(&msg);
        msg.type = MODBUS_COMMAND;
        msg.command = MODBUS_READ;
        msg.address = 0;
        msg.device_id = device_id;
        msg.data[0] = 1;

        if (!send_recv(serial, &msg) || msg.type == MODBUS_ERROR) {
            fprintf(stderr, "Error communicating with device\n");
            return 0;
        }

        if (msg.address != 9) {
            fprintf(stderr, "Invalid uzg status response size, expected = 9, got %d\n", msg.address);
            return 0;
        }

        for (int i = 0; i < msg.address; i++) {
            printf("%d", msg.data[i]);
            if (i != msg.address - 1) {
                printf(",");
            } else {
                printf("\n");
            }
        }

    }


    // stop uzg
    reset_msg(&msg);
    msg.type = MODBUS_COMMAND;
    msg.command = MODBUS_WRITE;
    msg.address = 0;
    msg.device_id = device_id;
    msg.data[0] = 0x0001;

    if (!send_recv(serial, &msg) || msg.type == MODBUS_ERROR) {
        fprintf(stderr, "Error communicating with device\n");
        return 0;
    }
}


int run_interactive(HANDLE serial) {
    char ch;
    modbus_msg_s msg;

	do
	{
        read_msg(&msg);

        if (send_recv(serial, &msg)) {
            print_msg(msg);
		}
		
        printf("Continue? (y/n) ");
		
	} while ((ch = getch()) != 'n');
}

