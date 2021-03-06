//
// Created by alex on 13.06.2020.
//
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#include "ipc.h"

typedef struct {
    pid_t pid; // special id for processes
    local_id localId; // id from ipc.h
    int *pipe_read; // who we need to READ from
    int *pipe_write; // who we need to WRITE into
}  process;



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

//    printf("Sending : process %d sent to process %d message: %s\n", sender->localId, dst, msg->s_payload);

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
                printf("Send = -1");
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
    process * receiver = self;


    int fd = receiver->pipe_read[from]; // where exactly we are sending!

    if ((read(fd, &msg->s_header, sizeof(MessageHeader))) == -1) {
        printf("Error!\n");
        return -1;
    }

    if ((read(fd, &msg->s_payload, msg->s_header.s_payload_len)) == -1) {
        printf("Error!\n");
        return -1;
    }

    printf("Receiving : Process %d received from process %d message : %s\n", receiver->localId, from, (char *) &msg->s_payload);

    return 0;
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
    process *process = self;
    int index_start = 1; // don't know if it is ok
//    if (process->localId == 0) { // it's parent   // you didn't send from 0 process!!!
//        index_start = 1;
//    }

    for (int index_pipe_read = index_start; index_pipe_read < number_of_processes; index_pipe_read++) {
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

