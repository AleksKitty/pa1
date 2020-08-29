#include <stdio.h>
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
#include "log.h"
#include "lamport.h"

extern int number_of_processes; // from input


//typedef struct {
//    pid_t pid; // special id for processes
//    local_id localId; // id from ipc.h
//    int *pipe_read; // who we need to READ from
//    int *pipe_write; // who we need to WRITE into
//    BalanceHistory balance_history; // struct for money and time of our process (Parent doesn't have money)
//}  process;



static int receive_from_all_children(void * self, Message * msg) {
    process *process = self;

    for (int index_pipe_read = 1; index_pipe_read < number_of_processes; index_pipe_read++) {
        if (index_pipe_read != process->localId) {

            receive(self, index_pipe_read, msg);
            doSecondRule(self, msg->s_header.s_local_time);
        }
    }

    return 0;
}


static void create_pipes(process *array_of_processes) {
    lg(0, "create_pipes", "Creating pipes!");

    FILE *pipe_log = fopen("pipes", "a"); // for writing into file
    for (int i = 0; i < number_of_processes; i++) {
        for (int j = 0; j < number_of_processes; j++) { // making pipes for everyone to everyone
            // try to create new pipe

            if (i != j) {
                // for standard pipe
                int fd[2];

                if (pipe(fd) < 0) { // fail
                    lg(0, "create_pipes", "Can't create new pipe");
                    exit(-1);
                }

                fcntl(fd[0], F_SETFL, fcntl(fd[0], F_GETFL) | O_NONBLOCK); // make pipes not blocking!!!
                fcntl(fd[1], F_SETFL, fcntl(fd[1], F_GETFL) | O_NONBLOCK); // make pipes not blocking!!!

                array_of_processes[j].pipe_read[i] = fd[0]; // j read from i
                array_of_processes[i].pipe_write[j] = fd[1]; // i write into j


                lg(0, "create_pipes", "Pipe (read %d, write %d) has OPENED", fd[0], fd[1]);
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

    Message message = {.s_header = {.s_type = BALANCE_HISTORY, .s_local_time = processik->lamport_time, .s_magic = MESSAGE_MAGIC},}; // our message, set s_header of Message; set s_type and s_magic of Header

    message.s_header.s_payload_len = (uint16_t) sizeof(BalanceHistory) + sizeof(BalanceState) * processik->balance_history.s_history_len; // set s_payload_len of Header

    memcpy(message.s_payload, &processik->balance_history, message.s_header.s_payload_len);

    send(processik, 0, &message); // sent to Parent HISTORY
}

static void update_balance_to_time(process* processik, timestamp_t current_time) {

    for (int time = processik->balance_history.s_history_len; time < current_time + 1; time++) {
        processik->balance_history.s_history[time].s_balance = processik->balance_history.s_history[time - 1].s_balance;
        processik->balance_history.s_history[time].s_balance_pending_in = processik->balance_history.s_history[time - 1].s_balance_pending_in;

//        lg(processik->localId, "update_balance_to_time","s_history[%d]{s_balance = %d, s_balance_pending_in = %d} ",
//                time,
//                processik->balance_history.s_history[time].s_balance,
//                processik->balance_history.s_history[time].s_balance_pending_in);

        processik->balance_history.s_history[time].s_time = time;
    }
    processik->balance_history.s_history_len = current_time + 1;

}

static void change_balances(process* processik, TransferOrder transferOrder, Message* message) {
    timestamp_t current_time = processik->lamport_time;


    lg(processik->localId, "change_balances", "process %d AFTER s_history_len = %d", processik->localId,
       processik->balance_history.s_history_len);
    lg(processik->localId, "change_balances", "process %d s_history[%d].s_balance = %d", processik->localId,
       current_time, processik->balance_history.s_history[current_time].s_balance);


    if (transferOrder.s_src == processik->localId) {
        processik->balance_history.s_history[current_time].s_balance -= transferOrder.s_amount;

        lg(processik->localId, "change_balances", "process %d minus transferOrder.s_amount = %d", processik->localId,
           transferOrder.s_amount);


        // First rule: update time before send or receive (time = time + 1)
        processik->lamport_time++;

        message->s_header.s_local_time = processik->lamport_time;

        send(processik, transferOrder.s_dst, message); // sent to another process message from Parent (money)

    } else if (transferOrder.s_dst == processik->localId) {
        processik->balance_history.s_history[current_time].s_balance += transferOrder.s_amount;

        lg(processik->localId, "change_balances", "process %d plus transferOrder.s_amount = %d", processik->localId,
           transferOrder.s_amount);


        for (timestamp_t i = message->s_header.s_local_time; i < processik->lamport_time; i++) {
            processik->balance_history.s_history[i].s_balance_pending_in += transferOrder.s_amount;
        }


        // First rule: update time before send or receive (time = time + 1)
        processik->lamport_time++;

        Message msg = {.s_header = {.s_type = ACK, .s_local_time = processik->lamport_time, .s_magic = MESSAGE_MAGIC},}; // our message, set s_header of Message; set s_type and s_magic of Header
        msg.s_header.s_payload_len = 0;


        send(processik, 0, &msg); // sent to Parent that we received Money from s_src (send ACK)
    }
}


static void create_processes(process *array_of_processes, FILE * event_log) {
    lg(0, "create_processes", "Creating processes:");

    for (int i = 1; i < number_of_processes; i++) {
        array_of_processes[i].localId = i; // give Local id to the future new process

        pid_t result_of_fork = fork();

        // try to create new processes
        if (result_of_fork == -1) {// fail
            lg(0, "create_processes", "Can't create new process");
            exit(-1);
        } else if (result_of_fork == 0) { // we are in child
            lg(array_of_processes[i].localId, "create_processes", "[son] pid %d from [parent] pid %d", getpid(),
               getppid());

            array_of_processes[i].pid = getpid(); // give pid

            close_unnecessary_pipes(array_of_processes, array_of_processes[i].localId); // struct is duplicated, we need to close unnecessary pipes!

            // First rule: update time before send or receive (time = time + 1)
            array_of_processes[i].lamport_time++;


            Message message = {.s_header = {.s_type = STARTED, .s_local_time = array_of_processes[i].lamport_time, .s_magic = MESSAGE_MAGIC},}; // our message, set s_header of Message; set s_type and s_magic of Header
            sprintf(message.s_payload, log_started_fmt, array_of_processes[i].lamport_time, array_of_processes[i].localId, array_of_processes[i].pid, getppid(), array_of_processes[i].balance_history.s_history[0].s_balance); // data of our message in a buffer, set s_payload of Message
            message.s_header.s_payload_len = (uint16_t) strlen(message.s_payload) + 1; // set s_payload_len of Header

            send_multicast(&array_of_processes[i], &message);

            // First rule: update time before send or receive (time = time + 1)
            array_of_processes[i].lamport_time++;

            // print
            fprintf(event_log, log_started_fmt, array_of_processes[i].lamport_time, i, getpid(), getppid(), array_of_processes[i].balance_history.s_history[0].s_balance);
            fflush(event_log);

            receive_from_all_children(&array_of_processes[i], &message); // receive all STARTED

            // print
            fprintf(event_log, log_received_all_started_fmt, array_of_processes[i].lamport_time, i);
            fflush(event_log);

            int done = 0;
            while(done < number_of_processes - 1) {

                // First rule: update time before send or receive (time = time + 1)
                array_of_processes[i].lamport_time++;

                memset(message.s_payload, 0, message.s_header.s_payload_len);
                receive_any(&array_of_processes[i], &message); // receive (at first step) TRANSFER from Parent

                // time = max of (time_receiver, time_sender)
                // time++
                doSecondRule(&array_of_processes[i], message.s_header.s_local_time); // check time


                if (message.s_header.s_type == TRANSFER) {

                    TransferOrder transferOrder;

                    update_balance_to_time(&array_of_processes[i], array_of_processes[i].lamport_time); // need to update time!

                    memcpy(&transferOrder, message.s_payload, message.s_header.s_payload_len); // get transferOrder from message buffer (memcpy = copy)
                    change_balances(&array_of_processes[i], transferOrder, &message);

                } else if (message.s_header.s_type == STOP) {

                    // First rule: update time before send or receive (time = time + 1)
                    array_of_processes[i].lamport_time++;

                    update_balance_to_time(&array_of_processes[i], array_of_processes[i].lamport_time); // need to update time!

                    // send DONE
                    message.s_header.s_type = DONE;
                    message.s_header.s_local_time = array_of_processes[i].lamport_time;
                    sprintf(message.s_payload, log_done_fmt, array_of_processes[i].lamport_time, array_of_processes[i].localId, array_of_processes[i].balance_history.s_history[array_of_processes[i].balance_history.s_history_len - 1].s_balance ); // data of our message in a buffer, set s_payload of Message
                    message.s_header.s_payload_len = (uint16_t) strlen(message.s_payload) + 1; // set s_payload_len of Header


                    send_multicast(&array_of_processes[i], &message); // send all DONE

                    done++;
                } else if (message.s_header.s_type == DONE) {
                    done++;
                }

            }
            // First rule: update time before send or receive (time = time + 1)
            array_of_processes[i].lamport_time++;

            update_balance_to_time(&array_of_processes[i], array_of_processes[i].lamport_time); // need to update time!

            send_history(&array_of_processes[i]);

            lg(array_of_processes[i].localId, "create_processes", "process %d exit!", array_of_processes[i].localId);

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
        lg(0, "main", "Wrong arguments!");
        lg(0, "main", "argc = %d", argc);
        exit(-1);
    }

    if (number_of_processes < 1 || number_of_processes > 10) { // checking number of processes
        lg(0, "main", "Wrong number of processes!");
        exit(-1);
    }

    number_of_processes++; // remember about Parent!

    balances = (int *) malloc(sizeof(int) * (number_of_processes)); // give memory to our array
    balances[0] = 0; // balance for Parent

    for (int i = 3; i < number_of_processes + 2; i++) { // check amount of money
        if (atoi(argv[i]) < 1 || atoi(argv[i]) > 99) {
            lg(0, "main", "Wrong amount of money!");
            exit(-1);
        }
        balances[i - 2] = atoi(argv[i]); // put from balance[1] etc
    }

    process array_of_processes[number_of_processes]; // put in an array, 0 process is a main parent process

    for (int i = 0; i < number_of_processes; i++) {
        array_of_processes[i].pipe_read = (int *) malloc(sizeof(int) * number_of_processes); // initialize our array
        array_of_processes[i].pipe_write = (int *) malloc(sizeof(int) * number_of_processes); // initialize our array
        array_of_processes[i].lamport_time = 0;
        array_of_processes[i].balance_history.s_history[0].s_time = 0; // put start time = 0

        array_of_processes[i].balance_history.s_history[0].s_balance = balances[i]; // put start balance
        array_of_processes[i].balance_history.s_history[0].s_balance_pending_in = 0; // put start balance_in_pending
        array_of_processes[i].balance_history.s_id = i;
        array_of_processes[i].balance_history.s_history_len = 1; // put start balance
    }

    lg(0, "main", "Number of processes = %d", number_of_processes);

    create_pipes(array_of_processes); // our function for creating all pipes

    array_of_processes[0].localId = PARENT_ID; // for parent process
    array_of_processes[0].pid = getpid(); // for parent process, get pid for current process

    FILE *event_log = fopen(events_log, "a"); // for writing into file

    create_processes(array_of_processes, event_log);

    close_unnecessary_pipes(array_of_processes, 0);

    // First rule: update time before any event (time = time + 1)
    array_of_processes[0].lamport_time++;

    Message message;
    receive_from_all_children(&array_of_processes[0], &message); // receive STARTED for PARENT GOOD!

    // print
    lg(0, "main", log_received_all_started_fmt, array_of_processes[0].lamport_time, 0);
    fprintf(event_log, log_received_all_started_fmt, array_of_processes[0].lamport_time, 0);
    fflush(event_log);

    // First rule: update time before send or receive event (time = time + 1)
    array_of_processes[0].lamport_time++;

    bank_robbery(&array_of_processes[0], number_of_processes - 1); // parent and number of children sending many TRANSFER


    // First rule: update time before send or receive event (time = time + 1)
    array_of_processes[0].lamport_time++;

    // send STOP
    message.s_header.s_type = STOP;
    message.s_header.s_local_time = array_of_processes[0].lamport_time;
    message.s_header.s_payload_len = 0; // set s_payload_len of Header
    send_multicast(&array_of_processes[0], &message);

    // First rule: update time before send or receive event (time = time + 1)
    array_of_processes[0].lamport_time++;

    receive_from_all_children(&array_of_processes[0], &message); // receive all DONE for PARENT


    AllHistory allHistory;
    allHistory.s_history_len = 0;
    for (int i = 1; i < number_of_processes; i++) {
        if (receive(&array_of_processes[0], i, &message) >= 0) { // receive HISTORY from all children

            doSecondRule(&array_of_processes[0], message.s_header.s_local_time);

            if (message.s_header.s_type == BALANCE_HISTORY) {

                memcpy(&allHistory.s_history[i - 1], message.s_payload, message.s_header.s_payload_len);
                allHistory.s_history_len++;
                //print_history(&allHistory);
            } else {
                perror("unexpected message");
            }
        }
    }

    lg(0, "main", "Received history!");

    print_history(&allHistory);

    fclose(event_log);


    for (local_id j = 1; j < number_of_processes; ++j) {
        wait(NULL);
    }

    sleep(1);

    close_all_pipes(array_of_processes);
}

