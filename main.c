#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "ipc.h"
#include "common.h"
#include "pa2345.h"
#include "banking.h"

extern int number_of_processes; // from input
//int processID;

typedef struct {
    pid_t pid; // special id for processes
    local_id localId; // id from ipc.h
    int *pipe_read; // who we need to READ from
    int *pipe_write; // who we need to WRITE into
    BalanceHistory balance_history; // struct for money and time of our process (Parent doesn't have money)
}  process;

static void log (pid_t p, const char * f, const char * m, ...){
    va_list args;
    va_start(args, m); // for reading arg
    printf("p:%d\t\tf:%s\t\tm:", p, f);
    vprintf(m, args);
    printf("\n");
    va_end(args);
}


static int receive_from_all_children(void * self, Message * msg) {
    process *process = self;

    for (int index_pipe_read = 1; index_pipe_read < number_of_processes; index_pipe_read++) {
        if (index_pipe_read != process->localId) {

            receive(self, index_pipe_read, msg);

        }
    }

    return 0;
}


static void create_pipes(process *array_of_processes) {
    log(0, "create_pipes", "Creating pipes!");

    FILE *pipe_log = fopen("pipes", "a"); // for writing into file
    for (int i = 0; i < number_of_processes; i++) {
        for (int j = 0; j < number_of_processes; j++) { // making pipes for everyone to everyone
            // try to create new pipe

            if (i != j) {
                // for standard pipe
                int fd[2];

                if (pipe(fd) < 0) { // fail
                    log(0, "create_pipes", "Can't create new pipe");
                    exit(-1);
                }

                fcntl(fd[0], F_SETFL, fcntl(fd[0], F_GETFL) | O_NONBLOCK); // make pipes not blocking!!!
                fcntl(fd[1], F_SETFL, fcntl(fd[1], F_GETFL) | O_NONBLOCK); // make pipes not blocking!!!

                array_of_processes[j].pipe_read[i] = fd[0]; // j read from i
                array_of_processes[i].pipe_write[j] = fd[1]; // i write into j


                log(0,"create_pipes", "Pipe (read %d, write %d) has OPENED", fd[0], fd[1]);
                fprintf(pipe_log, "Pipe (read %d, write %d) has OPENED", fd[0], fd[1]);
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

    for (int i = 0; i < number_of_processes; i++) {
        for (int j = 0; j < number_of_processes; j++) {
            if (i != id && i != j) {

                close(array_of_processes[i].pipe_write[j]); // i can't write into j

                close(array_of_processes[i].pipe_read[j]); // i can't read from j;
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

static void send_history(process* processik) {
    Message message = {.s_header = {.s_type = BALANCE_HISTORY, .s_local_time = get_physical_time(), .s_magic = MESSAGE_MAGIC},}; // our message, set s_header of Message; set s_type and s_magic of Header

    message.s_header.s_payload_len = (uint16_t) sizeof(BalanceHistory) + sizeof(BalanceState) * processik->balance_history.s_history_len; // set s_payload_len of Header

    memcpy(message.s_payload, &processik->balance_history, message.s_header.s_payload_len);

    send(processik, 0, &message); // sent to Parent HISTORY
}

static void update_balance_to_time(process* processik, timestamp_t current_time) {
        //log(processik->localId, "update_balance_to_time", "process %d my last amount s_history[%d].s_balance = %d", processik->localId, processik->balance_history.s_history_len - 1, processik->balance_history.s_history[processik->balance_history.s_history_len - 1].s_balance);
    for (int time = processik->balance_history.s_history_len; time < current_time + 1; time++) {
        processik->balance_history.s_history[time].s_balance = processik->balance_history.s_history[time - 1].s_balance;

        //log(processik->localId, "update_balance_to_time","process %d s_history[%d].s_balance = %d", processik->localId, time, processik->balance_history.s_history[time].s_balance);

        processik->balance_history.s_history[time].s_time = time;
        processik->balance_history.s_history_len = current_time + 1;
    }
}

static void change_balances(process* processik, TransferOrder transferOrder, Message* messageFromParent) {
    timestamp_t current_time = get_physical_time();
//    log(processik->localId, "change_balances", "current_time = %d", current_time);
//    log(processik->localId, "change_balances","process %d s_history_len = %d", processik->localId, processik->balance_history.s_history_len);

    update_balance_to_time(processik, current_time);

    log(processik->localId, "change_balances","process %d AFTER s_history_len = %d", processik->localId, processik->balance_history.s_history_len);
    log(processik->localId, "change_balances","process %d s_history[%d].s_balance = %d", processik->localId, current_time, processik->balance_history.s_history[current_time].s_balance);


    if (transferOrder.s_src == processik->localId) {
        processik->balance_history.s_history[current_time].s_balance -= transferOrder.s_amount;

        send(processik, transferOrder.s_dst, messageFromParent); // sent to another process message from Parent (money)

    } else if (transferOrder.s_dst == processik->localId) {
        processik->balance_history.s_history[current_time].s_balance += transferOrder.s_amount;

        Message message = {.s_header = {.s_type = ACK, .s_magic = MESSAGE_MAGIC},}; // our message, set s_header of Message; set s_type and s_magic of Header
        sprintf(message.s_payload, log_transfer_in_fmt, get_physical_time(),processik->localId, transferOrder.s_amount, transferOrder.s_src); // data of our message in a buffer, set s_payload of Message
        message.s_header.s_payload_len = (uint16_t) strlen(message.s_payload) + 1; // set s_payload_len of Header

        send(processik, 0, &message); // sent to Parent that we received Money from s_src (send ACK)
    }
}


static void create_processes(process *array_of_processes, FILE * event_log) {
    log(0, "create_processes", "Creating processes:");

    for (int i = 1; i < number_of_processes; i++) {
        array_of_processes[i].localId = i; // give Local id to the future new process

        pid_t result_of_fork = fork();

        // try to create new processes
        if (result_of_fork == -1) {// fail
            log(0,"create_processes", "Can't create new process");
            exit(-1);
        } else if (result_of_fork == 0) { // we are in child
            log(array_of_processes[i].localId, "create_processes", "[son] pid %d from [parent] pid %d", getpid(), getppid());

            array_of_processes[i].pid = getpid(); // give pid
            array_of_processes[i].balance_history.s_id = i; // set id of Balance_History
            array_of_processes[i].balance_history.s_history[i].s_time = get_physical_time(); //set time
            array_of_processes[i].balance_history.s_history[i].s_balance_pending_in = 0; //set time


            close_unnecessary_pipes(array_of_processes, array_of_processes[i].localId); // struct is duplicated, we need to close unnecessary pipes!


            Message message = {.s_header = {.s_type = STARTED, .s_local_time = get_physical_time(), .s_magic = MESSAGE_MAGIC},}; // our message, set s_header of Message; set s_type and s_magic of Header
            sprintf(message.s_payload, log_started_fmt, get_physical_time(), array_of_processes[i].localId, array_of_processes[i].pid, getppid(), array_of_processes[i].balance_history.s_history->s_balance); // data of our message in a buffer, set s_payload of Message
            message.s_header.s_payload_len = (uint16_t) strlen(message.s_payload) + 1; // set s_payload_len of Header

            send_multicast(&array_of_processes[i], &message);

            // print
            fprintf(event_log, log_started_fmt, get_physical_time(), i, getpid(), getppid(), array_of_processes[i].balance_history.s_history->s_balance);
            fflush(event_log);

            receive_from_all_children(&array_of_processes[i], &message); // receive all STARTED

            // print
            //log(array_of_processes[i].localId, "create_processes", log_received_all_started_fmt, get_physical_time(), i);
            fprintf(event_log, log_received_all_started_fmt, get_physical_time(), i);
            fflush(event_log);

            int in_cycle = 1;
            while(in_cycle == 1) {

                memset(message.s_payload, 0, message.s_header.s_payload_len);
                receive_any(&array_of_processes[i], &message); // receive (at first step) TRANSFER from Parent


                if (message.s_header.s_type == TRANSFER) {

                    TransferOrder transferOrder;

                    memcpy(&transferOrder, message.s_payload, message.s_header.s_payload_len); // get transferOrder from message buffer (memcpy = copy)
                    change_balances(&array_of_processes[i], transferOrder, &message);

                } else if (message.s_header.s_type == STOP) {

                    // send DONE
                    message.s_header.s_type = DONE;
                    message.s_header.s_local_time = get_physical_time();
                    sprintf(message.s_payload, log_done_fmt, get_physical_time(), array_of_processes[i].localId, array_of_processes[i].balance_history.s_history[array_of_processes[i].balance_history.s_history_len - 1].s_balance ); // data of our message in a buffer, set s_payload of Message
                    message.s_header.s_payload_len = (uint16_t) strlen(message.s_payload) + 1; // set s_payload_len of Header


                    update_balance_to_time(&array_of_processes[i], get_physical_time()); // need to update time!


                    send_multicast(&array_of_processes[i], &message); // send all DONE

                    receive_from_all_children(&array_of_processes[i], &message); // receive all DONE


                    send_history(&array_of_processes[i]);


                    in_cycle = -1;

                }

            }

            sleep(10);
            log(array_of_processes[i].localId, "create_processes","process %d exit!", array_of_processes[i].localId);


            exit(0);
        }
    }
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
        log(0, "main", "Wrong arguments!");
        log(0, "main", "argc = %d", argc);
        exit(-1);
    }

    if (number_of_processes < 1 || number_of_processes > 10) { // checking number of processes
        log(0, "main", "Wrong number of processes!");
        exit(-1);
    }

    number_of_processes++; // remember about Parent!

    balances = (int *) malloc(sizeof(int) * (number_of_processes)); // give memory to our array
    balances[0] = 0; // balance for Parent

    for (int i = 3; i < number_of_processes + 2; i++) { // check amount of money
        if (atoi(argv[i]) < 1 || atoi(argv[i]) > 99) {
            log(0, "main", "Wrong amount of money!");
            exit(-1);
        }
        balances[i - 2] = atoi(argv[i]); // put from balance[1] etc
    }

    process array_of_processes[number_of_processes]; // put in an array, 0 process is a main parent process

    for (int i = 0; i < number_of_processes; i++) {
        array_of_processes[i].pipe_read = (int *) malloc(sizeof(int) * number_of_processes); // initialize our array
        array_of_processes[i].pipe_write = (int *) malloc(sizeof(int) * number_of_processes); // initialize our array

        array_of_processes[i].balance_history.s_history[0].s_balance = balances[i]; // put start balance
        array_of_processes[i].balance_history.s_history_len = 1; // put start balance
    }

    log(0, "main","Number of processes = %d", number_of_processes);

    create_pipes(array_of_processes); // our function for creating all pipes

    array_of_processes[0].localId = PARENT_ID; // for parent process
    array_of_processes[0].pid = getpid(); // for parent process, get pid for current process

    FILE *event_log = fopen(events_log, "a"); // for writing into file

    create_processes(array_of_processes, event_log);

    close_unnecessary_pipes(array_of_processes, 0);

    Message message;
    receive_from_all_children(&array_of_processes[0], &message); // receive STARTED for PARENT GOOD!

    // print
    log(0, "main", log_received_all_started_fmt, get_physical_time(), 0);
    fprintf(event_log, log_received_all_started_fmt, get_physical_time(), 0);
    fflush(event_log);


    bank_robbery(&array_of_processes[0], number_of_processes - 1); // parent and number of children sending many TRANSFER


    // send STOP
    message.s_header.s_type = STOP;
    message.s_header.s_local_time = get_physical_time();
    message.s_header.s_payload_len = 0; // set s_payload_len of Header
    send_multicast(&array_of_processes[0], &message);

    receive_from_all_children(&array_of_processes[0], &message); // receive all DONE for PARENT


    AllHistory allHistory;
    allHistory.s_history_len = number_of_processes - 1; // number of children
    for (int i = 1; i < number_of_processes; i++) {
        if (receive(&array_of_processes[0], i, &message) >= 0) { // receive HISTORY from all children

            log(0, "main", "Received i = %d!, type = %d", i, message.s_header.s_type);

            if (message.s_header.s_type == BALANCE_HISTORY) {

                memcpy(&allHistory.s_history[i - 1], message.s_payload, message.s_header.s_payload_len);
            }

        }
    }

    sleep(1);

    log(0, "main", "Received history!");

    print_history(&allHistory);

    fclose(event_log);


    for (local_id j = 1; j < number_of_processes; ++j) {
        wait(NULL);
    }

    sleep(1);

    close_all_pipes(array_of_processes);
}


