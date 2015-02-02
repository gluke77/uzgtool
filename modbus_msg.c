#include <string.h>
#include <stdio.h>
#include "modbus_msg.h"

int lo_byte(int i) {
	return i & 0x000000FF;
}

int hi_byte(int i) {
	return (i & 0x0000FF00) >> 8;
}

int lo_tetr(int i) {
	return i & 0x0000000F;
}

int hi_tetr(int i) {
	return (i & 0x000000F0) >> 4;
}

char int2char(int i) {
	return (0x0A > lo_tetr(i)) ? lo_tetr(i) + '0' : lo_tetr(i) - 0x0A + 'A'; 
}

int char2int(char ch) {
	if (('0' <= ch) && (ch <= '9'))
		return ch - '0';
	else if (('A' <= ch) && (ch <= 'F'))
		return ch - 'A' + 0x0A;
	else
		return 0;
}

int add_char(char* buf, char ch) {
	if (':' == ch)
		memset(buf, 0, MODBUS_MAX_MSG_LENGTH);
	
	buf[strlen(buf)] = ch;
	
	return msg_ready(buf);
}

int msg_ready(char *buf) {
	int len = strlen(buf);

    if (len < 3)
		return 0;
		
	return (':' == buf[0]) && ('\r' == buf[len - 2]) && ('\n' == buf[len - 1]);
}

int crc(int *buf, int size) {
	int res = 0;
	
	for (int i = 0; i < size; i++)
		res += buf[i];

	res = 0xFF - lo_byte(res);
	res++;

	return res & 0x000000FF;
}

#define IBUF_LEN (200)

char* msg2str(modbus_msg_s msg, char *buf) {
	int ibuf[IBUF_LEN];
    int *ielem;
    
    ielem = ibuf;

	*ielem++ = msg.device_id;

	switch (msg.type) {
	case MODBUS_COMMAND:
		*ielem++ = msg.command;
	
		switch (msg.command)
		{
		case MODBUS_READ:
		case MODBUS_WRITE:
			*ielem++ = hi_byte(msg.address);
			*ielem++ = lo_byte(msg.address);
			*ielem++ = hi_byte(msg.data[0]);
			*ielem++ = lo_byte(msg.data[0]);
			break;
        default:
            fprintf(stderr, "Invalid modbus command %d\n", msg.command);
            return NULL;
		}
		break;
		
	case MODBUS_RESPONSE:
		*ielem++ = msg.command;
	
		switch (msg.command)
		{
		case MODBUS_WRITE:
			*ielem++ = hi_byte(msg.address);
			*ielem++ = lo_byte(msg.address);
			*ielem++ = hi_byte(msg.data[0]);
			*ielem++ = lo_byte(msg.data[0]);
			break;
		case MODBUS_READ:
			*ielem++ = lo_byte(msg.address);
            for (int i = 0; i < msg.address; i++) {
			    *ielem++ = hi_byte(msg.data[i]);
			    *ielem++ = lo_byte(msg.data[i]);
            }
			break;
        default:
            fprintf(stderr, "Invalid modbus command %d\n", msg.command);
            return NULL;
		}
		break;

	case MODBUS_ERROR:
		*ielem++ = msg.command | 0x80;
		*ielem++ = hi_byte(msg.data[0]);
		*ielem++ = lo_byte(msg.data[0]);
		break;
	}

	int checksum = crc(ibuf, ielem - ibuf);
	*ielem++ = checksum;
	
	memset(buf, 0x00, MODBUS_MAX_MSG_LENGTH);
	char *celem = buf;
    *celem++ = ':';

    int *iend = ielem;
    for(ielem = ibuf; ielem != iend; ielem++) {
		*celem++ = int2char(hi_tetr(*ielem));
		*celem++ = int2char(lo_tetr(*ielem));
	}

	*celem++ = '\r';
	*celem++ = '\n';
	
    return buf;
}

modbus_msg_s* str2msg(char *buf, modbus_msg_s *msg, int msg_type) {
    if (!msg) {
        return NULL;
    }

    reset_msg(msg);
    msg->type = msg_type;

	if (!msg_ready(buf)) {
		return NULL;
    }

	buf++; // skip leading ':'
    buf[strlen(buf) - 2] = 0; // chomp trailing '\r\n'

    int buflen = strlen(buf);

	if (0 != buflen % 2) {
		fprintf(stderr, "Invalid message length %d\n", buflen + 3);
		return NULL;
	}
	
	int ibuf[IBUF_LEN];
    int *ielem = ibuf;
	
    for (int bufidx = 0; bufidx < buflen;) { 
		*ielem = char2int(buf[bufidx++]) << 4;
		*ielem++ += char2int(buf[bufidx++]);
	}
		
    int expected_crc = *(--ielem);
    int calculated_crc = crc(ibuf, ielem - ibuf);

	if (expected_crc != calculated_crc) {
		fprintf(stderr, "Invalid message crc, expected=%d, calculated=%d\n", expected_crc, calculated_crc);
		return NULL;
	}
	
    
    int *iend = ielem;
    ielem = ibuf;

	msg->device_id = *ielem++;
	msg->command = *ielem++;

	if (0x80 & msg->command) {
		msg->command &= 0x7F;
		msg->type = MODBUS_ERROR;
	}

	switch (msg->command)
	{
	case MODBUS_READ:
		
		switch (msg->type) {
		
		case MODBUS_COMMAND:
			msg->address = *ielem++ << 8;
			msg->address += *ielem++;
			msg->data[0] = *ielem++ << 8;
            msg->data[0] += *ielem++;
			break;
		
		case MODBUS_RESPONSE:
			msg->address = *ielem++ / 2;
            for (int i = 0; i < msg->address; i++) {
			    msg->data[i] = *ielem++ << 8;
                msg->data[i] += *ielem++;
			}
			break;
	
		case MODBUS_ERROR:
			msg->data[0] = *ielem++ << 8;
            msg->data[0] += *ielem++;
			break;
		}
		break;

	case MODBUS_WRITE:
		
		
		switch (msg->type) {

		case MODBUS_COMMAND:
		case MODBUS_RESPONSE:
			msg->address = *ielem++ << 8;
			msg->address += *ielem++;
			msg->data[0] = *ielem++ << 8;
            msg->data[0] += *ielem++;
			break;
	
		case MODBUS_ERROR:
			msg->data[0] = *ielem++ << 8;
            msg->data[0] += *ielem++;
			break;
		}
		
		break;
	default:
		fprintf(stderr, "Invalid command code");
        return NULL;
	}
		
	/*
    if (ielem != iend) {
        fprintf(stderr, "Invalid message size");
        return NULL;
    }
    */
    return msg;
}

modbus_msg_s* read_msg(modbus_msg_s *msg) {
    if (!msg) {
        return NULL;
    }
    
    reset_msg(msg);

	printf("Enter\n");
	printf("\tmessage type (0-Command, 1-Response),\n");
	printf("\tcommand code (3-Read, 6-Write),\n");
	printf("\tdevice id,\n");
	printf("\taddress,\n");
	printf("\tdata\n");

    scanf("%d", &(msg->type));
    scanf("%d", &(msg->command));
    scanf("%d", &(msg->device_id));
    scanf("%d", &(msg->address));
    scanf("%d", &(msg->data[0]));

    return msg;
}

void print_msg(modbus_msg_s msg) {
    if (msg.type == MODBUS_RESPONSE && msg.command == MODBUS_READ) {
        printf("Modbus_msg: type=%u, command=%u, device_id=%u, data_size=%u, data=[ ",
            msg.type, msg.command, msg.device_id, msg.address);
        for (int i = 0; i < msg.address; i++)
            printf("%u ", msg.data[i]);
        printf("]\n");
    } else {
        printf("Modbus_msg: type=%u, command=%u, device_id=%u, address=%u, data=%u\n",
            msg.type, msg.command, msg.device_id, msg.address, msg.data[0]);
    }
}

void reset_msg(modbus_msg_s *msg) {
    memset(msg, 0, sizeof(modbus_msg_s));
}

