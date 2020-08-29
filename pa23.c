#include <string.h>
#include <sys/types.h>


#include "banking.h"
#include "ipc.h"
#include "lamport.h"


// sending and receiving Message with MONEY from one process to another
void transfer(void * parent_data, local_id src, local_id dst, balance_t amount){
//    printf("Transfer function:\n\n");

    process *parent_process = parent_data;

    // First rule: update time before send or receive event (time = time + 1)
    parent_process->balance_history.s_history->s_time++;

    Message message = {.s_header = {.s_type = TRANSFER, .s_local_time = parent_process->lamport_time, .s_magic = MESSAGE_MAGIC},}; // our message, set s_header of Message; set s_type and s_magic of Header

    TransferOrder transferOrder = {src, dst, amount}; // what we will put in a buffer of Message

    memcpy(message.s_payload, &transferOrder, sizeof(TransferOrder)); // put transfer in message buffer (memcpy = copy)
    message.s_header.s_payload_len = sizeof(TransferOrder); // set size of Message


    send(parent_process, src, &message); // send TRANSFER


    // First rule: update time before send or receive event (time = time + 1)
    parent_process->balance_history.s_history->s_time++;

    receive(parent_process, dst, &message); // receive ACK for PARENT

    doSecondRule(parent_process, message.s_header.s_local_time);
}

