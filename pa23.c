#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "banking.h"
#include "ipc.h"
#include "pa2345.h"

typedef struct {
    pid_t pid; // special id for processes
    local_id localId; // id from ipc.h
    int *pipe_read; // who we need to READ from
    int *pipe_write; // who we need to WRITE into
    balance_t balance; // amount of money of our process (Parent doesn't have money (balance = 0))
}  process;

// sending and receiving Message with MONEY from one process to another
void transfer(void * parent_data, local_id src, local_id dst, balance_t amount){
//    printf("Transfer function:\n\n");

    process *parent_process = parent_data;

    Message message = {.s_header = {.s_type = TRANSFER, .s_local_time = get_physical_time(), .s_magic = MESSAGE_MAGIC},}; // our message, set s_header of Message; set s_type and s_magic of Header

    TransferOrder transferOrder = {src, dst, amount}; // what we will put in a buffer of Message

    memcpy(message.s_payload, &transferOrder, sizeof(TransferOrder)); // put transfer in message buffer (memcpy = copy)
    message.s_header.s_payload_len = sizeof(TransferOrder); // set size of Message


    send(parent_process, src, &message); // send TRANSFER
    receive(parent_process, dst, &message); // receive ACK for PARENT
}


