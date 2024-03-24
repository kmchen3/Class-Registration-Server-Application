// Your helper functions need to be here.

#include "helpers.h"
#include "protocol.h"
#include <signal.h>

int UserComparator(const void *user1temp, const void *user2temp) {
    user_t *user1 = (user_t *)user1temp;
    user_t *user2 = (user_t *)user2temp;
    return strcmp(user1->username,user2->username);
}

int UsernameComparator(const void *user1temp, const void *user2temp) {
    char *user1 = (char *)user1temp;
    char *user2 = (char *)user2temp;
    return strcmp(user1,user2);
}

void UserPrinter(void *data, void *temp) {
    user_t *user = (user_t *)data;
    fprintf(stderr, "%s, %d, %d\n",user->username, user->enrolled, user->waitlisted);
}

void StatsPrinter() {
    fprintf(stderr, "%d, %d, %d, %d\n",curStats.clientCnt, curStats.threadCnt, curStats.totalAdds, curStats.totalDrops);
}

void UserDeleter(void *temp) {
    if (temp == NULL) {
        return;
    }
    user_t *freeUsernames = (user_t *)temp;
    if (freeUsernames->username == NULL) {
        return;
    }
    free(freeUsernames->username);
}

void CoursePrinter(course_t newData, void *temp) { 
    if (newData.title == NULL) {
        return;
    }
    int count = 1;
    fprintf(stdout, "%s, %d, %d, ", newData.title, newData.maxCap, newData.enrolledCnt);

    node_t *enrollmentNames = newData.enrollment->head;
    while (enrollmentNames != NULL) {
        if (count > 1) {
            fprintf(stdout, ";");
        }
        fprintf(stdout, "%s", (char *)enrollmentNames->data);
        count++;
        enrollmentNames = enrollmentNames->next;
    }
    fprintf(stdout, ", ");

    count = 1;
    node_t *waitlistNames = newData.waitlist->head;
    while (waitlistNames != NULL) {
        if (count > 1) {
            fprintf(stdout, ";");
        }
        fprintf(stdout, "%s", (char *)waitlistNames->data);
        count++;
        waitlistNames = waitlistNames->next;
    }
    fprintf(stdout, "\n");
}

void setStats() {
    curStats.clientCnt = 0;
    curStats.threadCnt = 0;
    curStats.totalAdds = 0;
    curStats.totalDrops = 0;
}

void setCourse(char *file) {
    FILE *openFile;
    int i = 0;
    char *line = NULL;
    size_t length = 0;
    const char s[2] = ";";
    char *token;
    openFile = fopen(file, "r");
    if (openFile == NULL) {
        perror("fopen");
        exit(2);
    }
    while (getline(&line, &length, openFile) != -1) {
        token = strtok(line, s);
        courseArray[i].title = malloc(strlen(token) + 1);
        strcpy(courseArray[i].title, token);

        token = strtok(NULL, s);
        courseArray[i].maxCap = atoi(token);

        courseArray[i].enrollment = CreateList(&UsernameComparator, NULL, NULL);
        courseArray[i].waitlist = CreateList(&UsernameComparator, NULL, NULL);
        i++;
        course_count++;
    }
    for (i; i < 32; i++) {
        courseArray[i].title = NULL;
    }
    free(line);
    fclose(openFile);
}

FILE *openLogFile(char *logFile) {
    FILE *openFile;
    openFile = fopen(logFile, "w");
    if (openFile == NULL) {
        perror("fopen");
        exit(2);
    }
    return openFile;
}

void kill_all_clients(list_t *temp) {
    node_t *num = (node_t *)temp->head;
    while (num != NULL) {
        if (((user_t *)num->data)->socket_fd >= 0) {
            pthread_kill(((user_t *)num->data)->tid, SIGINT);
            pthread_join(((user_t *)num->data)->tid, NULL);
        }
        num = num->next;
    }
}

user_t *verifyUsername(char *temp) {
    int exists = 0;
    int equals = 0;
    node_t *list = (node_t *)users->head;
    while (list != NULL) {
        user_start_reading((user_t *)(list->data));
        equals = strcmp(((user_t *)(list->data))->username, temp);
        user_done_reading((user_t *)(list->data));
        if (equals == 0) {
            exists = 1;
            break;
        }
        list = list->next;
    }
    if (exists == 1) {
        user_start_reading((user_t *)(list->data));
        if (((user_t *)(list->data))->socket_fd >= 0) { // if someone already logged into and active
            user_done_reading((user_t *)(list->data));
            pthread_mutex_lock(&logFile_lock);
            fprintf(logFile, "REJECT %s\n", temp);
            pthread_mutex_unlock(&logFile_lock);
            
            return NULL;
        }
        user_done_reading((user_t *)(list->data)); // if account exists but not logged in or active
        pthread_mutex_lock(&logFile_lock);
        fprintf(logFile, "RECONNECTED %s\n", temp);
        pthread_mutex_unlock(&logFile_lock);

        return (user_t *)(list->data);
    }
    pthread_mutex_lock(&logFile_lock);
    fprintf(logFile, "CONNECTED %s\n", temp);
    pthread_mutex_unlock(&logFile_lock);

    user_t *newUser = calloc(1, sizeof(user_t));
    newUser->username = calloc(1, strlen(temp) + 1);
    strcpy(newUser->username, temp);
    newUser->socket_fd = -1;
    newUser->tid = -1;
    newUser->enrolled = 0;
    newUser->waitlisted = 0;
    newUser->readcnt = 0;
    if (pthread_mutex_init(&(newUser->user_read_lock), NULL) != 0) {
        perror("pthread_mutex_init &(newUser->user_read_lock)");
        exit(2);
    }
    if (pthread_mutex_init(&(newUser->user_write_lock), NULL) != 0) {
        perror("pthread_mutex_init &(newUser->user_write_lock)");
        exit(2);
    }

    user_start_reading(newUser);
    InsertInOrder(users, (void *)newUser);
    user_done_reading(newUser);

    return newUser;
}

void printAllCourseArray() {
    void *temp = NULL;
    for (int i = 0; i < course_count; i++) {
        CoursePrinter(courseArray[i], temp);
    }
}

void user_start_reading(user_t *temp) {
    pthread_mutex_lock(&(temp->user_read_lock));
    temp->readcnt++;
    if (temp->readcnt == 1) {
        pthread_mutex_lock(&(temp->user_write_lock));
    }
    pthread_mutex_unlock(&(temp->user_read_lock));
}

void user_done_reading(user_t *temp) {
    pthread_mutex_lock(&(temp->user_read_lock));
    temp->readcnt--;
    if (temp->readcnt == 0) {
        pthread_mutex_unlock(&(temp->user_write_lock));
    }
    pthread_mutex_unlock(&(temp->user_read_lock));
}

char  *loadcList() {
    int total_size = 0;
    for (int i = 0; i < course_count; i++) {
        pthread_mutex_lock(&(course_locks[i]));
        char iToString[10];
        sprintf(iToString, "%d", i);
        total_size += 7 + strlen(iToString) + 3 + strlen(courseArray[i].title); // 7 x 3 = 21 + 9 = 30 + (1 x 3) = 33 + 17 + 6 + 8 = 64 
        if (courseArray[i].enrolledCnt == courseArray[i].maxCap) {
            total_size += strlen("(CLOSED)");
        }
        pthread_mutex_unlock(&(course_locks[i]));
    }

    char *result = (char *)calloc(1, sizeof(char) * total_size + course_count + 1); // + "\n"
    char course[8] = "Course ";
    char hyphen[4] = " - "; 
    for (int i = 0; i < course_count; i++) {
        pthread_mutex_lock(&(course_locks[i]));
        char iToString[2];
        sprintf(iToString, "%d", i);
        strcat(result, course); // Course
        strcat(result, iToString); // 0 1 2 3
        strcat(result, hyphen); // " - "
        strcat(result, courseArray[i].title); // "Let's Eat! French"
        if (courseArray[i].enrolledCnt == courseArray[i].maxCap) {
            strcat(result, "(CLOSED)");
        }
        strcat(result, "\n");
        pthread_mutex_unlock(&(course_locks[i]));
    }
    return result;
}

char *returnSched(uint32_t enrolled, uint32_t waitlisted) {
    char *result = (char *)calloc(1, sizeof(char));
    uint32_t mask = 1;
    int total_size = 0;
    char iToString[10];
    char course[8] = "Course ";
    char hyphen[4] = " - "; 
    char waiting[11] = " (WAITING)";
    for (int i = 0; i < course_count; i++) {
        pthread_mutex_lock(&(course_locks[i]));
        if ((enrolled & mask) == 1) {
            sprintf(iToString, "%d", i);
            total_size += strlen(course) + strlen(iToString) + strlen(hyphen) + strlen(courseArray[i].title) + 2; // 7 x 3 = 21 + 9 = 30 + (1 x 3) = 33 + 17 + 6 + 8 = 64 
            result = (char *)realloc(result, sizeof(char) * total_size);
            strcat(result, course); // Course
            strcat(result, iToString); // 0 1 2 3
            strcat(result, hyphen); // " - "
            strcat(result, courseArray[i].title); // "Let's Eat! French"
            strcat(result, "\n");
        }
        if ((waitlisted & mask) == 1) {
            sprintf(iToString, "%d", i);
            total_size += strlen(course) + strlen(iToString) + strlen(hyphen) + strlen(courseArray[i].title) + strlen(waiting) + 2;
            result = (char *)realloc(result, sizeof(char) * total_size);
            strcat(result, course); // Course
            strcat(result, iToString); // 0 1 2 3
            strcat(result, hyphen); // " - "
            strcat(result, courseArray[i].title); // "Let's Eat! French"
            strcat(result, waiting);
            strcat(result, "\n");
        }
        pthread_mutex_unlock(&(course_locks[i]));
        enrolled = enrolled >> 1;
        waitlisted = waitlisted >> 1;
    }
    return result;
}

int checkEnrolled(int index, uint32_t enrolled) {
    enrolled = enrolled >> index;
    uint32_t mask = 1;
    return (enrolled & mask);
}

uint32_t updateEnrollBitVector(uint32_t vector, int index) {
    uint32_t mask = 1;
    mask = mask << index;
    return (vector | mask);
}

uint32_t updateDropBitVector(uint32_t vector, int index) {
    uint32_t mask = 1 << index;
    mask = ~mask;
    return (vector & mask);
}