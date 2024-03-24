#include "server.h"
#include "protocol.h"
#include <pthread.h>
#include <signal.h>
#include "helpers.h"
#include <errno.h>

const char exit_str[] = "exit";

int total_num_msg = 0;
int listen_fd;

void sigint_handler(int sig) {
    flag = 1;
    // close(listen_fd);f
}

int server_init(int server_port){
    int sockfd;
    struct sockaddr_in servaddr;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(1);
    }
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(server_port);

    int opt = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (char *)&opt, sizeof(opt))<0) {
	    perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        printf("socket bind failed\n");
        exit(EXIT_FAILURE);
    }
    // else
    //     printf("Socket successfully binded\n");

    printf("Server initialized with %d courses.\n", course_count);

    // Now server is ready to listen and verification
    if ((listen(sockfd, 1)) != 0) {
        printf("Listen failed\n");
        exit(EXIT_FAILURE);
    }
    else {
        printf("Currently listening on port %d.\n", server_port);
    }
    return sockfd;
}

//Function running in thread
void *process_client(void* user_ptr){
    user_t *user = (user_t *)user_ptr;
    user_start_reading(user); // user write locks
    user->tid = pthread_self();
    user_done_reading(user); // user write unlock 
    pthread_mutex_lock(&(user->user_read_lock)); // user read lock
    int client_fd = user->socket_fd;
    pthread_mutex_unlock(&(user->user_read_lock)); // user read unlock
    fd_set read_fds;
    int retval;
    int received_size;
    int size = 0;
    char *buffer;
    petrV_header input;
    petrV_header output;

    while (flag == 0) {
        FD_ZERO(&read_fds);
        FD_SET(client_fd, &read_fds);
        retval = select(client_fd + 1, &read_fds, NULL, NULL, NULL);
        if (errno == EINTR && flag == 1) {
            printf("-1");
            break; //close(listen_fd);
        }
        if (retval!=1 && !FD_ISSET(client_fd, &read_fds)) {
            printf("Error with select() function\n");
            break;
        }
        if (rd_msgheader(client_fd, &input) == -1) {
            perror("rd_msgheader");
            close(client_fd);
            continue;
        }
        if (input.msg_type == LOGOUT) {
            memset(&output, 0, sizeof(petrV_header));
            output.msg_len = 0;
            output.msg_type = OK;
            if (wr_msg(client_fd, &output, NULL) == -1) {
                perror("wr_msg");
                close(client_fd);
                continue;
            }
            pthread_mutex_lock(&(user->user_read_lock));
            node_t *accessUsersRead = (node_t *)users->head;
            pthread_mutex_unlock(&(user->user_read_lock));

            user_start_reading(user);
            pthread_mutex_lock(&logFile_lock); // logFile_lock 
            fprintf(logFile, "%s LOGOUT\n", user->username);
            pthread_mutex_unlock(&logFile_lock); // logFile_lock
            user_done_reading(user);

            pthread_detach(pthread_self());

            user_start_reading(user); // user write lock
            user->socket_fd = -1;
            user_done_reading(user); // user write unlock
            break;
        }
        if (input.msg_type == CLIST) {
            memset(&output, 0, sizeof(petrV_header));
            output.msg_type = CLIST;

            void *temp = NULL;
            for (int i = 0; i < course_count; i++) {
                CoursePrinter(courseArray[i], temp);
            }
            char *result = loadcList();
            output.msg_len = strlen(result);
            if (wr_msg(client_fd, &output, result) == -1) {
                perror("wr_msg");
                close(client_fd);
                continue;
            }
            free(result);

            user_start_reading(user);
            pthread_mutex_lock(&logFile_lock);
            fprintf(logFile, "%s CLIST\n", user->username);
            pthread_mutex_unlock(&logFile_lock);
            user_done_reading(user);
        }
        
        if (input.msg_type == SCHED) {
            memset(&output, 0, sizeof(petrV_header));
            output.msg_len = 0;
            if (user->enrolled == 0 && user->waitlisted == 0) {
                output.msg_type = ENOCOURSES;
                if (wr_msg(client_fd, &output, NULL) == -1) {
                    perror("wr_msg");
                    close(client_fd);
                    continue;
                }
                user_start_reading(user);
                pthread_mutex_lock(&logFile_lock);
                fprintf(logFile, "%s NOSCHED\n", user->username);
                pthread_mutex_unlock(&logFile_lock);
                user_done_reading(user);
            }
            else {
                user_start_reading(user);
                char *check = returnSched(user->enrolled, user->waitlisted);
                user_done_reading(user);
                output.msg_len = strlen(check);
                output.msg_type = SCHED;
                if (wr_msg(client_fd, &output, check) == -1) {
                    perror("wr_msg");
                    close(client_fd);
                    continue;
                }
                user_start_reading(user);
                pthread_mutex_lock(&logFile_lock);
                fprintf(logFile, "%s SCHED\n", user->username);
                pthread_mutex_unlock(&logFile_lock);
                user_done_reading(user);
            }
        }
        
        if (input.msg_type == ENROLL) {
            pthread_mutex_lock(&curStats_lock);
            curStats.totalAdds++;
            pthread_mutex_unlock(&curStats_lock);
            memset(&output, 0, sizeof(petrV_header));
            output.msg_len = 0;
            buffer = malloc(input.msg_len);
            bzero(buffer, input.msg_len);
            received_size = read(client_fd, buffer, input.msg_len);
            if (flag == 1) {
                free(buffer);
                break; 
            }
            if(received_size < 0){
                printf("Receiving failed\n");
                break;
            }
            else if(received_size == 0){
                continue;
            }
            int index = atoi(buffer);
            free(buffer);
            if (index < 0 || index > course_count) {
                output.msg_len = 0;
                output.msg_type = ECNOTFOUND;
                if (wr_msg(client_fd, &output, NULL) == -1) {
                    perror("wr_msg");
                    close(client_fd);
                    continue;
                }
                user_start_reading(user);
                pthread_mutex_lock(&logFile_lock);
                fprintf(logFile, "%s NOTFOUND_E %d\n", user->username, index);
                pthread_mutex_unlock(&logFile_lock);
                user_done_reading(user);
                continue;
            }
            user_start_reading(user);
            if (checkEnrolled(index, user->enrolled) == 1) {
                output.msg_len = 0;
                output.msg_type = ECDENIED;
                if (wr_msg(client_fd, &output, NULL) == -1) {
                    perror("wr_msg");
                    close(client_fd);
                    continue;
                }
                pthread_mutex_lock(&logFile_lock);
                fprintf(logFile, "%s NOENROLL %d\n", user->username, index);
                pthread_mutex_unlock(&logFile_lock);
                continue;
            }
            user_done_reading(user);
            
            pthread_mutex_lock(&course_locks[index]);
            if (courseArray[index].enrolledCnt == courseArray[index].maxCap) {
                output.msg_len = 0;
                output.msg_type = ECDENIED;
                if (wr_msg(client_fd, &output, NULL) == -1) {
                    perror("wr_msg");
                    close(client_fd);
                    continue;
                }
                user_start_reading(user);
                pthread_mutex_lock(&logFile_lock);
                fprintf(logFile, "%s NOENROLL %d\n", user->username, index);
                pthread_mutex_unlock(&logFile_lock);
                user_done_reading(user);
                continue;
            }
            pthread_mutex_unlock(&course_locks[index]);

            pthread_mutex_lock(&course_locks[index]);
            user_start_reading(user);
            courseArray[index].enrolledCnt++;
            InsertInOrder(courseArray[index].enrollment, user->username);
            user_done_reading(user);
            pthread_mutex_unlock(&course_locks[index]);

            pthread_mutex_lock(&(user->user_write_lock));
            user->enrolled = updateEnrollBitVector(user->enrolled, index);
            pthread_mutex_unlock(&(user->user_write_lock));

            output.msg_len = 0;
            output.msg_type = OK;
            if (wr_msg(client_fd, &output, NULL) == -1) {
                perror("wr_msg");
                close(client_fd);
                continue;
            }

            user_start_reading(user);
            pthread_mutex_lock(&logFile_lock);
            fprintf(logFile, "%s ENROLL %d %d\n", user->username, index, user->enrolled);
            pthread_mutex_unlock(&logFile_lock);
            user_done_reading(user);
            continue;
        }

        if (input.msg_type == WAIT) {
            memset(&output, 0, sizeof(petrV_header));
            output.msg_len = 0;
            buffer = malloc(input.msg_len);
            bzero(buffer, input.msg_len);
            received_size = read(client_fd, buffer, input.msg_len);
            if (flag == 1) {
                free(buffer);
                break; 
            }
            if(received_size < 0){
                printf("Receiving failed\n");
                break;
            }
            else if(received_size == 0){
                continue;
            }
            int index = atoi(buffer);
            free(buffer);
            if (index < 0 || index > course_count) {
                output.msg_len = 0;
                output.msg_type = ECNOTFOUND;
                if (wr_msg(client_fd, &output, NULL) == -1) {
                    perror("wr_msg");
                    close(client_fd);
                    continue;
                }
                user_start_reading(user);
                pthread_mutex_lock(&logFile_lock);
                fprintf(logFile, "%s NOTFOUND_W %d\n", user->username, index);
                pthread_mutex_unlock(&logFile_lock);
                user_done_reading(user);
                continue;
            }

            pthread_mutex_lock(&course_locks[index]);
            if (courseArray[index].enrolledCnt < courseArray[index].maxCap) {
                output.msg_len = 0;
                output.msg_type = ECDENIED;
                if (wr_msg(client_fd, &output, NULL) == -1) {
                    perror("wr_msg");
                    close(client_fd);
                    continue;
                }
                user_start_reading(user);
                pthread_mutex_lock(&logFile_lock);
                fprintf(logFile, "%s NOWAIT %d\n", user->username, index);
                pthread_mutex_unlock(&logFile_lock);
                user_done_reading(user);
                continue;
            }
            pthread_mutex_unlock(&course_locks[index]);

            user_start_reading(user);
            if (checkEnrolled(index, user->enrolled) == 1 || checkEnrolled(index, user->waitlisted) == 1) {
                output.msg_len = 0;
                output.msg_type = ECDENIED;
                if (wr_msg(client_fd, &output, NULL) == -1) {
                    perror("wr_msg");
                    close(client_fd);
                    continue;
                }
                pthread_mutex_lock(&logFile_lock);
                fprintf(logFile, "%s NOWAIT %d\n", user->username, index);
                pthread_mutex_unlock(&logFile_lock);
                continue;
            }
            user_done_reading(user);

            pthread_mutex_lock(&course_locks[index]);
            user_start_reading(user);
            InsertAtTail(courseArray[index].waitlist, user->username);
            user_done_reading(user);
            pthread_mutex_unlock(&course_locks[index]);

            pthread_mutex_lock(&(user->user_write_lock));
            user->waitlisted = updateEnrollBitVector(user->waitlisted, index);
            pthread_mutex_unlock(&(user->user_write_lock));

            output.msg_len = 0;
            output.msg_type = OK;
            if (wr_msg(client_fd, &output, NULL) == -1) {
                perror("wr_msg");
                close(client_fd);
                continue;
            }
            user_start_reading(user);
            pthread_mutex_lock(&logFile_lock);
            fprintf(logFile, "%s WAIT %d %d\n", user->username, index, user->waitlisted);
            pthread_mutex_unlock(&logFile_lock);
            user_done_reading(user);
        }

        if (input.msg_type == DROP) {
            memset(&output, 0, sizeof(petrV_header));
            output.msg_len = 0;
            buffer = malloc(input.msg_len);
            bzero(buffer, input.msg_len);
            received_size = read(client_fd, buffer, input.msg_len);
            if (flag == 1) {
                free(buffer);
                break; 
            }
            if(received_size < 0){
                printf("Receiving failed\n");
                break;
            }
            else if(received_size == 0){
                continue;
            }
            int index = atoi(buffer);
            free(buffer);
            if (index < 0 || index > course_count) {
                output.msg_len = 0;
                output.msg_type = ECNOTFOUND;
                if (wr_msg(client_fd, &output, NULL) == -1) {
                    perror("wr_msg");
                    close(client_fd);
                    continue;
                }
                user_start_reading(user);
                pthread_mutex_lock(&logFile_lock);
                fprintf(logFile, "%s NOTFOUND_D %d\n", user->username, index);
                pthread_mutex_unlock(&logFile_lock);
                user_done_reading(user);
                continue;
            }

            user_start_reading(user);
            if (checkEnrolled(index, user->enrolled) != 1) {
                output.msg_len = 0;
                output.msg_type = ECDENIED;
                if (wr_msg(client_fd, &output, NULL) == -1) {
                    perror("wr_msg");
                    close(client_fd);
                    continue;
                }
                pthread_mutex_lock(&logFile_lock);
                fprintf(logFile, "%s NODROP %d\n", user->username, index);
                pthread_mutex_unlock(&logFile_lock);
                continue;
            }
            user_done_reading(user);

            user_start_reading(user);
            if (checkEnrolled(index, user->enrolled) == 1) {
                pthread_mutex_lock(&curStats_lock);
                curStats.totalDrops++;
                pthread_mutex_unlock(&curStats_lock);

                pthread_mutex_lock(&course_locks[index]);
                courseArray[index].enrolledCnt--;
                int i = 0;
                node_t *hold = ((node_t *)(courseArray[index].enrollment)->head);
                while (hold != NULL) {
                    if (strcmp(((char *)hold->data), user->username) == 0) {
                        break;
                    }
                    i++;
                    hold = hold->next;
                }
                void *removedUser = RemoveByIndex(courseArray[index].enrollment, i); //remove enrolled person from the list
                pthread_mutex_unlock(&course_locks[index]);

                // pthread_mutex_lock(&(user->user_write_lock));
                user->enrolled = updateDropBitVector(user->enrolled, index);
                // pthread_mutex_unlock(&(user->user_write_lock));

                output.msg_len = 0;
                output.msg_type = OK;
                if (wr_msg(client_fd, &output, NULL) == -1) {
                    perror("wr_msg");
                    close(client_fd);
                    continue;
                }
                pthread_mutex_lock(&logFile_lock);
                fprintf(logFile, "%s DROP %d %d\n", user->username, index, user->enrolled);
                pthread_mutex_unlock(&logFile_lock);
            }
            user_done_reading(user);

            pthread_mutex_lock(&(course_locks[index]));
            if (courseArray[index].waitlist->head != NULL) {
                pthread_mutex_lock(&curStats_lock);
                curStats.totalAdds++;
                pthread_mutex_unlock(&curStats_lock);

                void *newEnrolled = RemoveFromHead(courseArray[index].waitlist);
                InsertInOrder(courseArray[index].enrollment, newEnrolled);

                user_start_reading(user);
                node_t *person = (node_t *)(users->head);
                user_done_reading(user);

                while (person != NULL) {
                    if (strcmp(((user_t *)person->data)->username, (char *)newEnrolled) == 0) {
                        pthread_mutex_lock(&(((user_t *)person->data))->user_write_lock);
                        ((user_t *)person->data)->enrolled = updateEnrollBitVector(((user_t *)person->data)->enrolled, index);
                        ((user_t *)person->data)->waitlisted = updateDropBitVector(((user_t *)person->data)->waitlisted, index);
                        pthread_mutex_unlock(&(((user_t *)person->data))->user_write_lock);
                        courseArray[index].enrolledCnt++;
                        break;
                    }
                    person = person->next;
                }
                user_start_reading(((user_t *)person->data));
                pthread_mutex_lock(&logFile_lock);
                fprintf(logFile, "%s WAITADD %d %d\n", ((user_t *)person->data)->username, index, ((user_t *)person->data)->enrolled);
                pthread_mutex_unlock(&logFile_lock);
                user_done_reading(((user_t *)person->data));
            }
            pthread_mutex_unlock(&(course_locks[index]));
        }
    }
    close(user->socket_fd);
    return NULL;
}

void run_server(int server_port){
    listen_fd = server_init(server_port); // Initiate server and start listening on specified port
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    pthread_t tid;

    int received_size;
    fd_set read_fds;
    int retval;
    petrV_header input;
    petrV_header output;
    int size = 0;

    char *buffer;
    flag = 0;
    while(flag == 0){
        // Wait and Accept the connection from client
       // printf("Wait for new client connection\n");
        client_fd = accept(listen_fd, (SA*)&client_addr, &client_addr_len);
        if (flag == 1) {
            break; //close(listen_fd);
        }
        if (client_fd < 0) {
            printf("server acccept failed\n");
            exit(EXIT_FAILURE);
        }
        else{
            //printf("Client connetion accepted\n");
            FD_ZERO(&read_fds);
            FD_SET(client_fd, &read_fds);
            retval = select(client_fd + 1, &read_fds, NULL, NULL, NULL);
            if (flag == 1) {
                break;
            }
            if (retval!=1 && !FD_ISSET(client_fd, &read_fds)) {
                printf("Error with select() function\n");
                break;
            }
            if (rd_msgheader(client_fd, &input) == -1) {
                perror("rd_msgheader");
                close(client_fd);
                continue;
            }
            if (input.msg_type == LOGIN) {
                pthread_mutex_lock(&curStats_lock); // curStats lock
                curStats.clientCnt++;
                pthread_mutex_unlock(&curStats_lock); // curStats unlock
                buffer = malloc(input.msg_len);
                bzero(buffer, input.msg_len);
                received_size = read(client_fd, buffer, input.msg_len);
                if (flag == 1) {
                    break; 
                }
                if(received_size < 0){
                    printf("Receiving failed\n");
                    break;
                }
                else if(received_size == 0){
                    continue;
                }
                user_t *verifiedUser = verifyUsername(buffer);
                if (verifiedUser == NULL) {
                    free(buffer);
                    output.msg_len = 0;
                    output.msg_type = EUSRLGDIN;
                    if (wr_msg(client_fd, &output, NULL) == -1) {
                        perror("wr_msg");
                        close(client_fd);
                        continue;
                    }
                }
                
                user_start_reading(verifiedUser);
                verifiedUser->socket_fd = client_fd;
                user_done_reading(verifiedUser);

                memset(&output, 0, sizeof(petrV_header));
                output.msg_len = 0;
                output.msg_type = OK;
                if (wr_msg(client_fd, &output, NULL) == -1) {
                    perror("wr_msg");
                    close(client_fd);
                    continue;
                }
                pthread_create(&tid, NULL, process_client, verifiedUser);
                
                pthread_mutex_lock(&curStats_lock); // curStats lock
                curStats.threadCnt++;
                pthread_mutex_unlock(&curStats_lock);// curStats unlock
                free(buffer);
            }
        }   
    }
    kill_all_clients(users);
    printAllCourseArray();
    PrintLinkedList(users, stderr);
    StatsPrinter();
    DeleteList(users);
    free(users);
    for (int i = 0; i < course_count; i++) {
        free(courseArray[i].title);
    }
    close(listen_fd);
    return;
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "h")) != -1) {
        switch (opt) {
            case 'h':
                fprintf(stderr, USAGE_MSG);
                exit(EXIT_FAILURE);
        }
    }

    // 3 positional arguments necessary
    if (argc != 4) {
        fprintf(stderr, USAGE_MSG);
        exit(EXIT_FAILURE);
    }
    unsigned int port_number = atoi(argv[1]);
    char * course_filename = argv[2];
    char * log_filename = argv[3];

    //INSERT CODE HERE

    if (port_number == 0){
        fprintf(stderr, "ERROR: Port number for server to listen is not given\n");
        fprintf(stderr, "Server Application Usage: %s -p <port_number>\n",
                    argv[0]);
        exit(EXIT_FAILURE);
    }

    users = CreateList(&UserComparator, &UserPrinter, &UserDeleter);
    // linked list of user_t structs
    // [0] username, socket, tid... [1] username, socket, tid... [2]... etc
    setStats();
    //setUser(users);
    course_count = 0;
    setCourse(course_filename);
    logFile = openLogFile(log_filename);

    struct sigaction myaction = {{0}};
    myaction.sa_handler = sigint_handler;

    if (sigaction(SIGINT, &myaction, NULL) == -1) {
        printf("signal handler failed to install\n");
    }

    if (pthread_mutex_init(&curStats_lock, NULL) != 0) {
        perror("pthread_mutex_init curStats_lock");
        exit(2);
    }

    if (pthread_mutex_init(&logFile_lock, NULL) != 0) {
        perror("pthread_mutex_init logFile_lock");
        exit(2);
    }

    for (int i = 0; i < 32; i++) {
        if (pthread_mutex_init(&course_locks[i], NULL) != 0) {
            perror("pthread_mutex_init");
            exit(2);
        }
    }
    run_server(port_number);

    return 0;
}