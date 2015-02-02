#ifndef _MODBUSMSG_INCLUDED
#define _MODBUSMSG_INCLUDED

#define MODBUS_READ     (3)
#define MODBUS_WRITE    (6)

#define MODBUS_COMMAND  (0)
#define MODBUS_RESPONSE (1)
#define MODBUS_ERROR    (2)

#define MODBUS_MAX_MSG_LENGTH	(92) // MAX(17, 11 + 4 * N) + 1, N - max words to read
#define MODBUS_READ_REQ_LENGTH	(17)
#define MODBUS_MAX_WORDS_READ	(20)

typedef struct {
    int device_id;
    int type;
    int command;
    int address;
    int data[MODBUS_MAX_WORDS_READ];
} modbus_msg_s;


int add_char(char* buf, char ch);
int msg_ready(char *buf);
char* msg2str(modbus_msg_s msg, char *buf);
modbus_msg_s* str2msg(char *buf, modbus_msg_s *msg, int msg_type);
modbus_msg_s* read_msg(modbus_msg_s *msg);
void print_msg(modbus_msg_s msg);
void reset_msg(modbus_msg_s *msg);

#endif /* _MODBUSMSG_INCLUDED */
