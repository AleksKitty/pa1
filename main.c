#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "ipc.h"
#include "common.h"
#include "pa1.h"
#include "banking.h"

extern int number_of_processes; // from input


typedef struct {
    pid_t pid; // special id for processes
    local_id localId; // id from ipc.h
    int *pipe_read; // who we need to READ from
    int *pipe_write; // who we need to WRITE into
    balance_t balance; // amount of money of our process (Parent doesn't have money (balance = 0))
}  process;



static void create_pipes(process *array_of_processes) {
    printf("Creating pipes!\n");

    FILE *pipe_log = fopen("pipes", "a"); // for writing into file
    for (int i = 0; i < number_of_processes; i++) {
        for (int j = 0; j < number_of_processes; j++) { // making pipes for everyone to everyone
            // try to create new pipe

            if (i != j) {
                // for standard pipe
                int fd[2];

                if (pipe(fd) < 0) { // fail
                    printf("Can't create new pipe\n");
                    exit(-1);
                }

                array_of_processes[j].pipe_read[i] = fd[0]; // j read from i
                array_of_processes[i].pipe_write[j] = fd[1]; // i write into j

                printf("Pipe (read %d, write %d) has OPENED\n", fd[0], fd[1]);
                fprintf(pipe_log, "Pipe (read %d, write %d) has OPENED\n", fd[0], fd[1]);
                fflush(pipe_log);
            } else {
                array_of_processes[j].pipe_read[i] = -1; // can't read from itself
                array_of_processes[i].pipe_write[j] = -1; // can't write into itself
            }
        }

    }
    fclose(pipe_log);
}

static void close_unnecessary_pipes(process array_of_processes[], local_id id) {

    //printf("id = %d\n", id);
    for (int i = 0; i < number_of_processes; i++) {
        for (int j = 0; j < number_of_processes; j++) {
            if (i != id && i != j) {

                //printf("i = %d, j = %d\n", i, j);
                close(array_of_processes[i].pipe_write[j]); // i can't write into j
                //printf("%d can't write into %d; pipe_write[%d] = %d\n", i, j, j, array_of_processes[i].pipe_write[j]);

                close(array_of_processes[i].pipe_read[j]); // i can't read from j;
                //printf("%d can't read from %d; pipe_read[%d] = %d\n", i, j, j, array_of_processes[i].pipe_read[j]);
            }
        }
    }
}


static void close_all_pipes(process array_of_processes[]) {
    for (int i = 0; i < number_of_processes; i++) {
        for (int j = 0; j < number_of_processes; j++) {
            if (i != j) {
                if (array_of_processes[j].pipe_read[i] > 0) {
                    close(array_of_processes[j].pipe_read[i]); // j read from i
                }

                if (array_of_processes[i].pipe_read[j] > 0) {
                    close(array_of_processes[i].pipe_read[j]); // j read from i
                }
            }
        }
    }
}


static void create_processes(process *array_of_processes) {
    FILE *event_log = fopen(events_log, "a"); // for writing into file
    printf("Creating processes:\n");

    for (int i = 1; i < number_of_processes; i++) {
        array_of_processes[i].localId = i; // give Local id to the future new process

        pid_t result_of_fork = fork();

        // try to create new processes
        if (result_of_fork == -1) {// fail
            printf("Can't create new process\n");
            exit(-1);
        } else if (result_of_fork == 0) { // we are in child
            array_of_processes[i].pid = getpid(); // give pid
            printf("[son] pid %d from [parent] pid %d\n", getpid(), getppid());


            close_unnecessary_pipes(array_of_processes, array_of_processes[i].localId); // struct is duplicated, we need to close unnecessary pipes!


            Message message = {.s_header = {.s_type = STARTED, .s_magic = MESSAGE_MAGIC},}; // our message, set s_header of Message; set s_type and s_magic of Header
            sprintf(message.s_payload, log_started_fmt, array_of_processes[i].localId, array_of_processes[i].pid, getppid()); // data of our message in a buffer, set s_payload of Message
            message.s_header.s_payload_len = (uint16_t) strlen(message.s_payload) + 1; // set s_payload_len of Header

            send_multicast(&array_of_processes[i], &message);

            // print
            fprintf(event_log, log_started_fmt, i, getpid(), getppid());
            fflush(event_log);

            receive_any(&array_of_processes[i], &message);

            // print
            printf(log_received_all_started_fmt, i);
            fprintf(event_log, log_received_all_started_fmt, i);
            fflush(event_log);


            // send and receive DONE
            message.s_header.s_type = DONE;
            sprintf(message.s_payload, log_done_fmt, array_of_processes[i].localId); // data of our message in a buffer, set s_payload of Message
            message.s_header.s_payload_len = (uint16_t) strlen(message.s_payload) + 1; // set s_payload_len of Header
            send_multicast(&array_of_processes[i], &message);

            // print
            fprintf(event_log, log_done_fmt, i);
            fflush(event_log);

            receive_any(&array_of_processes[i], &message);

            // print
            printf(log_received_all_done_fmt, i);
            fprintf(event_log, log_received_all_done_fmt, i);
            fflush(event_log);

            fclose(event_log);
            exit(0);
        }
    }

    close_unnecessary_pipes(array_of_processes, 0);

    Message message;
    receive_any(&array_of_processes[0], &message); // receive for PARENT

    // print
    printf(log_received_all_started_fmt, 0);
    fprintf(event_log, log_received_all_started_fmt, 0);
    fflush(event_log);

    receive_any(&array_of_processes[0], &message); // receive for PARENT

    printf(log_received_all_done_fmt, 0);
    fprintf(event_log, log_received_all_started_fmt, 0);
    fflush(event_log);

}

int main(int argc, char *argv[]) {

    int right_arguments = -1;
    int* balance;
    if (argc >= 4 && strcmp("-p", argv[1]) == 0) { // reading input parameters
        number_of_processes = atoi(argv[2]);

        if (argc == number_of_processes + 3) {
            right_arguments = 0;
        }
    }
    if (right_arguments != 0) {
        printf("Wrong arguments!\n");
        printf("argc = %d\n", argc);
        exit(-1);
    }

    if (number_of_processes < 1 || number_of_processes > 10) { // checking number of processes
        printf("Wrong number of processes!\n");
        exit(-1);
    }

    number_of_processes++; // remember about Parent!

    balance = (int *) malloc(sizeof(int) * (number_of_processes)); // give memory to our array
    balance[0] = 0; // balance for Parent

    for (int i = 3; i < number_of_processes + 2; i++) { // check amount of money
        if (atoi(argv[i]) < 1 || atoi(argv[i]) > 99) {
            printf("Wrong amount of money!\n");
            exit(-1);
        }
        balance[i - 2] = atoi(argv[i]); // put from balance[1] etc
    }

    process array_of_processes[number_of_processes]; // put in an array, 0 process is a main parent process

    for (int i = 0; i < number_of_processes; i++) {
        array_of_processes[i].pipe_read = (int *) malloc(sizeof(int) * number_of_processes); // initialize our array
        array_of_processes[i].pipe_write = (int *) malloc(sizeof(int) * number_of_processes); // initialize our array

        array_of_processes[i].balance = balance[i];
        printf("balance: %d\n", balance[i]);
    }

    printf("Number of processes = %d\n", number_of_processes);

    create_pipes(array_of_processes); // our function for creating all pipes

    array_of_processes[0].localId = PARENT_ID; // for parent process
    array_of_processes[0].pid = getpid(); // for parent process, get pid for current process


    create_processes(array_of_processes);
    sleep(1);

    for (local_id j = 1; j < number_of_processes; ++j) {
        wait(NULL);
    }

    close_all_pipes(array_of_processes);
}


