//
// Created by alex on 28.08.2020.
//

#include "banking.h"
#include "lamport.h"
#include "log.h"

// time = max of (time_receiver, time_sender)
// time++
//
void doSecondRule(process * process_receiver, timestamp_t msg_time) {

    if(get_lamport_time(&process_receiver) < msg_time) {
        process_receiver->lamport_time = msg_time;
    }

    process_receiver->lamport_time++;

    lg(process_receiver->localId, "doSecondRule", "process %d lamport_time = %d", process_receiver->localId, process_receiver->lamport_time);
}
