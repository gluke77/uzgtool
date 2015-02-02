#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <unistd.h>
#include "serial.h"
#include "modbus_msg.h"

modbus_msg_s* recv_modbus_msg(HANDLE serial, modbus_msg_s *msg);
int send_modbus_msg(HANDLE serial, modbus_msg_s msg);
modbus_msg_s* send_recv(HANDLE serial, modbus_msg_s *msg);
int range_scan(HANDLE serial, int device_id, int lo, int hi, int step);
int run_interactive(HANDLE serial);
void parse_args(int argc, char *argv[]);
void din_read(HANDLE serial, int device_id);
void din_prog(HANDLE serial, int device_id, char *din);

char *g_port = 0;
char *g_baudrate = 0;
int g_device_id = 0;
int g_low_freq = 0;
int g_high_freq = 0;
int g_step = 1;
int g_din_read = 0;
char *g_din_prog = 0;
int g_verbose = 0;

void usage() {
    printf("Usage: usbtool -p <port> -b <baudrate> -d <device_id> -l <lo_freq> -h <hi_freq> -s <step> -r -w <DIN> -v\n");
}


int main(int argc, char * argv[])
{
    parse_args(argc, argv);

    if (!g_port || !g_baudrate) {
        usage();
        exit(-1);
    }

    HANDLE serial = init_serial(g_port, g_baudrate);
    if (serial == INVALID_HANDLE_VALUE) {
        return -1;
    }

    if (g_din_read && g_device_id) {
        din_read(serial, g_device_id);
    } else if (g_din_prog && g_device_id) {
        din_prog(serial, g_device_id, g_din_prog);
    } else if (g_device_id && g_low_freq && g_high_freq) {
        range_scan(serial, g_device_id, g_low_freq, g_high_freq, g_step);
    } else {
        usage();
        printf("Running interactive mode\n");
        g_verbose = 1;
        run_interactive(serial);
    }

	if ( !CloseHandle(serial) )
		fprintf(stderr, "Error closing serial port\n");

	return 0;
}

void parse_args(int argc, char *argv[]) {
    int c;

    while ((c = getopt(argc, argv, "p:b:d:l:h:s:rw:v")) != -1) {
        switch (c) {
        case 'p' :
            g_port = optarg;
            break;
        case 'b' :
            g_baudrate = optarg;
            break;
        case 'd' :
            g_device_id = atoi(optarg);
            break;
        case 'l' :
            g_low_freq = atoi(optarg);
            break;
        case 'h' :
            g_high_freq = atoi(optarg);
            break;
        case 's' :
            g_step = atoi(optarg);
            break;
        case 'r' :
            g_din_read = 1;
            break;
        case 'w' :
            g_din_prog = optarg;
            break;
        case 'v' :
            g_verbose = 1;
            break;
        default:
            usage();
            exit(-1);
        }
    }
}

modbus_msg_s* recv_modbus_msg(HANDLE serial, modbus_msg_s *msg) {
    char buf[MODBUS_MAX_MSG_LENGTH];
    char ch;

    while (!msg_ready(buf)) {
        if (read_char(serial, &ch)) {
            add_char(buf, ch);
            if (g_verbose)
                printf("%c", ch);
        }
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

    if (g_verbose) {
        printf("Sending:");
        print_msg(msg);
    }

    if (!msg2str(msg, buf)) {
        fprintf(stderr, "Error converting modbus message to string\n");
        return 0;
    }

    if (g_verbose) 
        printf("Sending:%s", buf);

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

void din_read(HANDLE serial, int device_id) {
    modbus_msg_s msg;
    
    // get din
    reset_msg(&msg);
    msg.type = MODBUS_COMMAND;
    msg.command = MODBUS_READ;
    msg.address = 32;
    msg.device_id = device_id;
    msg.data[0] = 1;

    if (!send_recv(serial, &msg) || msg.type == MODBUS_ERROR) {
        fprintf(stderr, "Error communicating with device\n");
        return;
    }

    if (g_verbose)
        print_msg(msg);

    for (int i = 0; i < msg.address; i++)
        printf("%c", msg.data[i]);

    printf("\n");
}

void din_prog(HANDLE serial, int device_id, char *din) {
    modbus_msg_s msg;
    
    for (int i = 0; i < 16; i++) {
        reset_msg(&msg);
        msg.type = MODBUS_COMMAND;
        msg.command = MODBUS_WRITE;
        msg.address = 32 + i;
        msg.device_id = device_id;
        msg.data[0] = din[i];

        if (!send_recv(serial, &msg) || msg.type == MODBUS_ERROR) {
            fprintf(stderr, "Error communicating with device\n");
            return;
        }

        if (g_verbose)
            print_msg(msg);

        Sleep(100);

    }
}
