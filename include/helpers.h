#include "server.h"
#include <stdlib.h>
#include "linkedlist.h"
#include "protocol.h"

int UserComparator(const void *second1, const void *second2);
int UsernameComparator(const void *user1temp, const void *user2temp);
void UserPrinter(void *data, void *temp);
void UserDeleter(void *temp);
void StatsPrinter();
void CoursePrinter(course_t newData, void *temp);

int server_init(int server_port);
void *process_client(void* clientfd_ptr);
void run_server(int server_port);

void setStats();
void setUser(list_t *temp);
void setCourse(char *file);
FILE *openLogFile(char *logFile);
void sigint_to_clients(user_t *user);
void kill_all_clients(list_t *temp);
user_t *verifyUsername(char *temp);
void printAllCourseArray();
void user_start_reading(user_t *temp);
void user_done_reading(user_t *temp);
char *loadcList();
char *returnSched(uint32_t enrolled, uint32_t waitlisted);
int checkEnrolled(int index, uint32_t enrolled);
uint32_t updateEnrollBitVector(uint32_t vector, int index);
uint32_t updateDropBitVector(uint32_t vector, int index);
