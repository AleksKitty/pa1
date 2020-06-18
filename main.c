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
    BalanceHistory balance_history; // struct for money and time of our process (Parent doesn't have money)
}  process;

static int receive_any_classic(void * self, Message * msg) {
    process *process = self;

    for (int index_pipe_read = 1; index_pipe_read < number_of_processes; index_pipe_read++) {
        if (index_pipe_read != process->localId) {
            int result = receive(self, index_pipe_read, msg);

            if (result == -1) {
                printf("Receive = -1\n");
                return -1;
            }
        }
    }

    return 0;
}


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

static void change_balances(process processik, TransferOrder *transferOrder, Message *messageFromParent) {
    timestamp_t current_time = get_physical_time();

    for (int time = processik.balance_history.s_history_len; time < current_time + 1; time++) {
        processik.balance_history.s_history[time].s_balance = processik.balance_history.s_history[time - 1].s_balance;
        processik.balance_history.s_history[time].s_time = time;
    }



    processik.balance_history.s_history[current_time].s_time = current_time;
    if (transferOrder->s_src == processik.localId) {
        processik.balance_history.s_history[current_time].s_balance -= transferOrder->s_amount;

        send(&processik, transferOrder->s_dst, messageFromParent); // sent to Parent that we received Money from s_src (send ACK)

    } else if (transferOrder->s_dst == processik.localId) {
        processik.balance_history.s_history[current_time].s_balance += transferOrder->s_amount;

        Message message = {.s_header = {.s_type = ACK, .s_magic = MESSAGE_MAGIC},}; // our message, set s_header of Message; set s_type and s_magic of Header
        sprintf(message.s_payload, log_started_fmt, processik.localId, processik.pid, getppid()); // data of our message in a buffer, set s_payload of Message
        message.s_header.s_payload_len = (uint16_t) strlen(message.s_payload) + 1; // set s_payload_len of Header
        send(&processik, 0, &message); // sent to Parent that we received Money from s_src (send ACK)
    }

}


static void create_processes(process *array_of_processes, const int *balances) {
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
            printf("[son] pid %d from [parent] pid %d\n", getpid(), getppid());

            array_of_processes[i].pid = getpid(); // give pid
            array_of_processes[i].balance_history.s_id = i; // set id of Balance_History

            array_of_processes[i].balance_history.s_history_len = 1; // size of array (amount of T)
            array_of_processes[i].balance_history.s_history[i].s_balance = 0; // start time
            array_of_processes[i].balance_history.s_history[i].s_time = get_physical_time(); // put input money in structure


            close_unnecessary_pipes(array_of_processes, array_of_processes[i].localId); // struct is duplicated, we need to close unnecessary pipes!


            Message message = {.s_header = {.s_type = STARTED, .s_local_time = get_physical_time(), .s_magic = MESSAGE_MAGIC},}; // our message, set s_header of Message; set s_type and s_magic of Header
            sprintf(message.s_payload, log_started_fmt, array_of_processes[i].localId, array_of_processes[i].pid, getppid()); // data of our message in a buffer, set s_payload of Message
            message.s_header.s_payload_len = (uint16_t) strlen(message.s_payload) + 1; // set s_payload_len of Header

            send_multicast(&array_of_processes[i], &message);

            // print
            fprintf(event_log, log_started_fmt, i, getpid(), getppid());
            fflush(event_log);

            receive_any_classic(&array_of_processes[i], &message); // receive STARTED

            // print
            printf(log_received_all_started_fmt, i);
            fprintf(event_log, log_received_all_started_fmt, i);
            fflush(event_log);

            int in_cycle = 1;
            while(in_cycle == 1) {

                TransferOrder transferOrder;
//            printf((const char *) message.s_header.s_type);
                memset(message.s_payload, 0, message.s_header.s_payload_len);
                receive_any(&array_of_processes[i], &message); // receive TRANSFER from Parent

                printf("\n");

                if (message.s_header.s_type == TRANSFER) {
                    printf("Message type = TRANSFER\n");
                    memcpy(&transferOrder, message.s_payload,
                           message.s_header.s_payload_len); // get transferOrder from message buffer (memcpy = copy)
                    change_balances(array_of_processes[i], &transferOrder, &message);
                }


//            // send and receive DONE
//            message.s_header.s_type = DONE;
//            sprintf(message.s_payload, log_done_fmt, array_of_processes[i].localId); // data of our message in a buffer, set s_payload of Message
//            message.s_header.s_payload_len = (uint16_t) strlen(message.s_payload) + 1; // set s_payload_len of Header
//            send_multicast(&array_of_processes[i], &message);
//
//            // print
//            fprintf(event_log, log_done_fmt, i);
//            fflush(event_log);
//
//            receive_any(&array_of_processes[i], &message);
//
//            // print
//            printf(log_received_all_done_fmt, i);
//            fprintf(event_log, log_received_all_done_fmt, i);
//            fflush(event_log);

                in_cycle = -1;

            }

            fclose(event_log);
            exit(0);
        }
    }

    Message message;
    receive_any_classic(&array_of_processes[0], &message); // receive STARTED for PARENT

    // print
    printf(log_received_all_started_fmt, 0);
    fprintf(event_log, log_received_all_started_fmt, 0);
    fflush(event_log);

    bank_robbery(&array_of_processes[0], number_of_processes - 1); // parent and number of children sending many TRANSFER

    printf("Received ACK");

//    receive_any(&array_of_processes[0], &message); // receive DONE for PARENT
//
//    printf(log_received_all_done_fmt, 0);
//    fprintf(event_log, log_received_all_started_fmt, 0);
//    fflush(event_log);

    fclose(event_log);
}

int main(int argc, char *argv[]) {

    int right_arguments = -1;
    int* balances;
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

    balances = (int *) malloc(sizeof(int) * (number_of_processes)); // give memory to our array
    balances[0] = 0; // balance for Parent

    for (int i = 3; i < number_of_processes + 2; i++) { // check amount of money
        if (atoi(argv[i]) < 1 || atoi(argv[i]) > 99) {
            printf("Wrong amount of money!\n");
            exit(-1);
        }
        balances[i - 2] = atoi(argv[i]); // put from balance[1] etc
    }

    process array_of_processes[number_of_processes]; // put in an array, 0 process is a main parent process

    for (int i = 0; i < number_of_processes; i++) {
        array_of_processes[i].pipe_read = (int *) malloc(sizeof(int) * number_of_processes); // initialize our array
        array_of_processes[i].pipe_write = (int *) malloc(sizeof(int) * number_of_processes); // initialize our array

        if (i != 0) {
            array_of_processes[i].balance_history.s_history->s_balance = balances[i]; // put start balance
        }
        printf("balance: %d\n", array_of_processes[i].balance_history.s_history->s_balance);
    }

    printf("Number of processes = %d\n", number_of_processes);

    create_pipes(array_of_processes); // our function for creating all pipes

    array_of_processes[0].localId = PARENT_ID; // for parent process
    array_of_processes[0].pid = getpid(); // for parent process, get pid for current process


    create_processes(array_of_processes, balances);
    sleep(1);

    for (local_id j = 1; j < number_of_processes; ++j) {
        wait(NULL);
    }

    close_all_pipes(array_of_processes);
}


