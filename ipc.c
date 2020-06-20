//
// Created by alex on 13.06.2020.
//
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>

#include "ipc.h"

typedef struct {
    pid_t pid; // special id for processes
    local_id localId; // id from ipc.h
    int *pipe_read; // who we need to READ from
    int *pipe_write; // who we need to WRITE into
}  process;

static void log (pid_t p, const char * f, const char * m, ...){
    va_list args;
    va_start(args, m); // for reading arg
    printf("p:%d\t\tf:%s\t\tm:", p, f);
    vprintf(m, args);
    printf("\n");
    va_end(args);
}



int number_of_processes;

//------------------------------------------------------------------------------
/** Send a message to the process specified by id.
 *
 * @param self    Any data structure implemented by students to perform I/O
 * @param dst     ID of recepient
 * @param msg     Message to send
 *
 * @return 0 on success, any non-zero value on error
 */

int send(void *self, local_id dst, const Message *msg) {
    process *sender = self;

    int fd = sender->pipe_write[dst];

    if (write(fd, msg, sizeof(MessageHeader) + msg->s_header.s_payload_len) == -1) {
        perror("Error\n");
        return -1;
    }

    if (msg->s_header.s_type == TRANSFER) {
        log(sender->localId, "send", "Sending : process %d sent to process %d message: %s\n", sender->localId, dst, "\"TRANSFER\"");
    } else if (msg->s_header.s_type == ACK) {
        log(sender->localId, "send","Sending : process %d sent to process %d message: %s\n", sender->localId, dst, (char *) msg->s_payload);
    } else if (msg->s_header.s_type == STOP) {
        log(sender->localId, "send","Sending : process %d sent to process %d message: %s\n", sender->localId, dst, "\"STOP\"");
    } else if ( msg->s_header.s_type == DONE) {
        log(sender->localId, "send","Sending : process %d sent to process %d message: %s\n", sender->localId, dst, (char *) msg->s_payload);
    } else if ( msg->s_header.s_type == BALANCE_HISTORY) {
        log(sender->localId, "send","Sending : process %d sent to process %d message: %s\n", sender->localId, dst, "\"HISTORY\"");
    }
    return 0;
}

//------------------------------------------------------------------------------

/** Send multicast message.
 *
 * Send msg to all other processes including parrent.
 * Should stop on the first error.
 *
 * @param self    Any data structure implemented by students to perform I/O
 * @param msg     Message to multicast.
 *
 * @return 0 on success, any non-zero value on error
 */
int send_multicast(void * self, const Message * msg) {
    process *process = self;
    for (int i = 0; i < number_of_processes; i++) {
        if (i != process->localId) {

            if (send(self, i, msg) == -1) {
                log(process->localId, "send_multicast","Send = -1");
                return -1;
            }
        }
    }
    return 0;
}

//------------------------------------------------------------------------------

/** Receive a message from the process specified by id.
 *
 * Might block depending on IPC settings.
 *
 * @param self    Any data structure implemented by students to perform I/O
 * @param from    ID of the process to receive message from
 * @param msg     Message structure allocated by the caller
 *
 * @return 0 on success, any non-zero value on error
 */
int receive(void * self, local_id from, Message * msg) {
    process *receiver = self;

    int fd = receiver->pipe_read[from]; // where exactly we are sending!

    while (1) {
        int read_result = read(fd, &msg->s_header, sizeof(MessageHeader));
        log(receiver->localId, "receive", "read_header_res = %d", read_result);

        if (read_result > 0) {

            while (1) {
                int result = read(fd, &msg->s_payload, msg->s_header.s_payload_len);
                log(receiver->localId, "receive", "read_msg_res = %d", result);
                if (result >= 0) {

                    if (msg->s_header.s_type == TRANSFER) {
                        log(receiver->localId, "receive", "Process %d received from process %d message : %s", receiver->localId, from, "\"TRANSFER\"");
                    } else if (msg->s_header.s_type == STARTED) {
                        log(receiver->localId, "receive", "Process %d received from process %d message : %s", receiver->localId,
                               from, (char *) &msg->s_payload);
                    } else if (msg->s_header.s_type == BALANCE_HISTORY) {
                        log(receiver->localId, "receive", "Process %d received from process %d message : %s\n", receiver->localId,
                               from, "\"HISTORY\"");
                    } else if (msg->s_header.s_type == DONE) {
                        log(receiver->localId, "receive", "Process %d received from process %d message : %s\n", receiver->localId,
                               from, (char *) &msg->s_payload);
                    } else if (msg->s_header.s_type == ACK) {
                        log(receiver->localId, "receive", "Process %d received from process %d message : %s\n", receiver->localId,
                               from, "\"ACK\"");
                    }

                    return result;

                } else {
                    sleep(1);
                }
            }
        } else {
            sleep(1);
        }
    }
}


//------------------------------------------------------------------------------

/** Receive a message from any process.
 *
 * Receive a message from any process, in case of blocking I/O should be used
 * with extra care to avoid deadlocks.
 *
 * @param self    Any data structure implemented by students to perform I/O
 * @param msg     Message structure allocated by the caller
 *
 * @return 0 on success, any non-zero value on error
 */
int receive_any(void * self, Message * msg) {
    process *processik = self;

    while (1) {

        for (int index_pipe_read = 0; index_pipe_read < number_of_processes; index_pipe_read++) {

            if(index_pipe_read == processik->localId) {
                continue;
            }

            int result = read(processik->pipe_read[index_pipe_read], &msg->s_header, sizeof(MessageHeader));

            log(processik->localId,"receive_any", "read_header_res = %d", result);

            if (result > 0) {

                if (read(processik->pipe_read[index_pipe_read], &msg->s_payload, msg->s_header.s_payload_len) >= 0) {

                    if (msg->s_header.s_type == TRANSFER) {
                        log(processik->localId, "receive", "Process %d received from process %d message : %s", processik->localId, processik->pipe_read[index_pipe_read], "\"TRANSFER\"");
                    } else if (msg->s_header.s_type == STARTED) {
                        log(processik->localId, "receive", "Process %d received from process %d message : %s", processik->localId,
                            processik->pipe_read[index_pipe_read], (char *) &msg->s_payload);
                    } else if (msg->s_header.s_type == BALANCE_HISTORY) {
                        log(processik->localId, "receive", "Process %d received from process %d message : %s\n", processik->localId,
                            processik->pipe_read[index_pipe_read], "\"HISTORY\"");
                    } else if (msg->s_header.s_type == DONE) {
                        log(processik->localId, "receive", "Process %d received from process %d message : %s\n", processik->localId,
                            processik->pipe_read[index_pipe_read], (char *) &msg->s_payload);
                    } else if (msg->s_header.s_type == ACK) {
                        log(processik->localId, "receive", "Process %d received from process %d message : %s\n", processik->localId,
                            processik->pipe_read[index_pipe_read], "\"ACK\"");
                    }

                    return 0;
                }
            }
        }
    }
}



