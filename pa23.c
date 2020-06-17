#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "banking.h"
#include "ipc.h"
#include "common.h"
#include "pa1.h"
#include "banking.h"

typedef struct {
    pid_t pid; // special id for processes
    local_id localId; // id from ipc.h
    int *pipe_read; // who we need to READ from
    int *pipe_write; // who we need to WRITE into
    balance_t balance; // amount of money of our process (Parent doesn't have money (balance = 0))
}  process;

// sending and receiving Message with MONEY from one process to another
void transfer(void * parent_data, local_id src, local_id dst, balance_t amount){
    Message message = {.s_header = {.s_type = TRANSFER, .s_magic = MESSAGE_MAGIC},}; // our message, set s_header of Message; set s_type and s_magic of Header
    TransferOrder transferOrder = {src, dst, amount}; // what we will put in a buffer of Message

    message.s_header.s_local_time = get_physical_time(); // set time in Message
    message.s_header.s_payload_len = sizeof(transferOrder) + 1; // set size of Message

    memcpy(message.s_payload, &transferOrder, message.s_header.s_payload_len); // put transfer in message buffer (memcpy = copy)

    send(parent_data, dst, &message);
    receive(parent_data, dst, &message);
}
