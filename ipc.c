//
// Created by alex on 13.06.2020.
//
#include <stdio.h>
#include <unistd.h>

#include "ipc.h"
#include "log.h"
#include "banking.h"

typedef struct {
    pid_t pid; // special id for processes
    local_id localId; // id from ipc.h
    int *pipe_read; // who we need to READ from
    int *pipe_write; // who we need to WRITE into
    BalanceHistory balance_history; // struct for money and time of our process (Parent doesn't have money)
}  process;



int number_of_processes;

static int has_printable_payload(MessageType msgType) {
    switch (msgType) {
        case STARTED:
        case DONE:
            return 1;
        default:
            return 0;
    }
}

static const char* get_msg_type_name(MessageType msgType) {
    switch (msgType) {
        case STARTED:
            return "STARTED";
        case DONE:
            return "DONE";
        case ACK:
            return "ACK";
        case STOP:
            return "STOP";
        case TRANSFER:
            return "TRANSFER";
        case BALANCE_HISTORY:
            return "BALANCE_HISTORY";
        case CS_REQUEST:
            return "CS_REQUEST";
        case CS_REPLY:
            return "CS_REPLY";
        case CS_RELEASE:
            return "CS_RELEASE";
        default:
            return "unknown";
    }
}

static void lg_msg(pid_t p, const char * f, const Message *msg, local_id src, local_id dst) {
    const MessageHeader* h = &msg->s_header;
    const char * type = get_msg_type_name(h->s_type);
    if (h->s_payload_len > 0 && has_printable_payload(h->s_type)) {
        const char * m = "src = %d; dst = %d; type = %s; payload-size = %d; payload = %s";
        lg(p, f, m, src, dst, type, h->s_payload_len, msg->s_payload);
    } else {
        const char * m = "src = %d; dst = %d; message = %s; payload-size = %d";
        lg(p, f, m, src, dst, type, h->s_payload_len);
    }
}

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

    lg_msg(sender->localId, "send", msg, sender->localId, dst);

    if (write(fd, msg, sizeof(MessageHeader) + msg->s_header.s_payload_len) == -1) {
        perror("Error\n");
        return -1;
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
                lg(process->localId, "send_multicast", "Send = -1");
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
 * @return  1 on success, -1 on error
 */
int receive(void * self, local_id from, Message * msg) {
    process *receiver = self;

    int fd = receiver->pipe_read[from]; // where exactly we are sending!

    while (1) {
        int read_header_res = read(fd, &msg->s_header, sizeof(MessageHeader));
        lg(receiver->localId, "receive", "read_header_res = %d from %d", read_header_res, from);

        if (read_header_res > 0) {
            lg(receiver->localId, "receive", "s_payload_len = %d from %d", msg->s_header.s_payload_len, from);

            if (msg->s_header.s_payload_len > 0) {
                int read_payload_res = read(fd, &msg->s_payload, msg->s_header.s_payload_len);
                lg(receiver->localId, "receive", "read_payload_res = %d from %d", read_payload_res, from);
            }
            lg_msg(receiver->localId, "receive", msg, from, receiver->localId);
            return 1;
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
 * @return 1 on success, -1 on error
 */
int receive_any(void * self, Message * msg) {
    process *processik = self;

    while (1) {

        for (int index_pipe_read = 0; index_pipe_read < number_of_processes; index_pipe_read++) {

            if(index_pipe_read == processik->localId) {
                continue;
            }

            int read_header_res = read(processik->pipe_read[index_pipe_read], &msg->s_header, sizeof(MessageHeader));

            lg(processik->localId, "receive_any", "read_header_res = %d from %d", read_header_res, index_pipe_read);

            if (read_header_res > 0) {
                lg(processik->localId, "receive_any", "s_payload_len = %d from %d", msg->s_header.s_payload_len, index_pipe_read);

                if (msg->s_header.s_payload_len > 0) {
                    int read_payload_res = read(processik->pipe_read[index_pipe_read], &msg->s_payload, msg->s_header.s_payload_len);
                    lg(processik->localId, "receive_any", "read_payload_res = %d from %d", read_payload_res, index_pipe_read);
                }
                lg_msg(processik->localId, "receive_any", msg, index_pipe_read, processik->localId);
                return 1;
            } else {
                sleep(1);
            }
        }
    }
}


