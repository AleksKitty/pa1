//
// Created by alex on 28.08.2020.
//

#ifndef PA1_LAMPORT_H
#define PA1_LAMPORT_H

#include <sys/types.h>
#include "banking.h"

typedef struct {
    pid_t pid; // special id for processes
    local_id localId; // id from ipc.h
    int *pipe_read; // who we need to READ from
    int *pipe_write; // who we need to WRITE into
    BalanceHistory balance_history; // struct for money and time of our process (Parent doesn't have money)
}  process;

void doSecondRule(process * process_receiver, timestamp_t msg_time);

#endif //PA1_LAMPORT_H
