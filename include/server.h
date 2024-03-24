#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "linkedlist.h"
#include <pthread.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define SA struct sockaddr

#define USAGE_MSG "./bin/zotReg_server [-h] PORT_NUMBER COURSE_FILENAME LOG_FILENAME"\
                  "\n  -h                 Displays this help menu and returns EXIT_SUCCESS."\
                  "\n  PORT_NUMBER        Port number to listen on."\
                  "\n  COURSE_FILENAME    File to read course information from at the start of the server"\
                  "\n  LOG_FILENAME       File to output server actions into. Create/overwrite, if exists\n"


typedef struct {
    int clientCnt;  // # of attempted logins (successful & unsuccessful)
    int threadCnt;  // # of threads created (successful login)
    int totalAdds;  // # of enrolled actions for all courses 
        // enroll request and when user is enrolled from waitlist
    int totalDrops; // # of drops for all courses (successful course drops only)
} stats_t;   //stats collected since server start-up

stats_t curStats; // lock with mutex
pthread_mutex_t curStats_lock; // = PTHREAD_MUTEX_INITIALIZER; 

typedef struct {
    char* username;	// pointer to dynamically allocated string that you create
    int socket_fd; // >= 0 if active on server, set to -1 if not active
    pthread_t tid; // current thread id, only used if active on server
    pthread_mutex_t user_read_lock;
    pthread_mutex_t user_write_lock;
    int readcnt;
    uint32_t enrolled; // bit vector storage for courses that user has enrolled in 
    uint32_t waitlisted; // bit vector storage for courses that user is waitlisted for
} user_t;

list_t *users;

typedef struct {
    char* title; // pointer to dynamically allocated string that you create
    int   maxCap; // maximum enrollment for the course
    int   enrolledCnt; // added
    list_t *enrollment; // linked list of enrolled users (alphabetically), bounded length to maxCap
    list_t *waitlist; // linked list of waitlisted users (InsertAtTail), unbounded length  
} course_t; 
pthread_mutex_t course_locks[32];

course_t courseArray[32]; 
    // one course per index, stored lowest to highest index
    // set title pointer to NULL if course does not exist
    // maximum of 32 courses on the server

FILE *logFile; 

int flag;
int course_count;
char *course_list;

pthread_mutex_t logFile_lock; // = PTHREAD_MUTEX_INITIALIZER; 

// INSERT FUNCTIONS HERE

#endif
