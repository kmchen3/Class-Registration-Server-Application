#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// These are the message types for the PetrV protocol
enum msg_types {
    OK,
    LOGIN,
    LOGOUT,
    CLIST,
    SCHED,
    ENROLL,
    DROP,
    WAIT,
    EUSRLGDIN = 0xF0,
    ECDENIED,
    ECNOTFOUND,
    ENOCOURSES,
    ESERV = 0xFF
};

// This is the struct describes the header of the PetrV protocol messages
typedef struct {
    uint32_t msg_len; // this should include the null terminator
    uint8_t msg_type; 
        // 3 bytes of padding
} petrV_header; // struct size of 8 bytes

// read a petr_header from socket_fd
// places contents into memory referenced by h
// returns 0 upon success, -1 upon error
int rd_msgheader(int socket_fd, petrV_header *h);

// write petr_header reference by h and the msgbuf to socket_fd
// if msg_len in h = 0, msgbuf is ignored
// returns 0 upon success, -1 upon error
int wr_msg(int socket_fd, petrV_header *h, char *msgbuf);

#endif