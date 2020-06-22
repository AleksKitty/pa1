/**
 * @file     bank_robbery.c
 * @Author   Michael Kosyakov and Evgeniy Ivanov (ifmo.distributedclass@gmail.com)
 * @date     March, 2014
 * @brief    Toy implementation of bank_robbery(), don't do it in real life ;)
 *
 * Students must not modify this file!
 */

#include <stdio.h>

#include "banking.h"

void bank_robbery(void * parent_data, local_id max_id)
{
    printf("!Bank robbery has started!\n\n");
    for (int i = 1; i < max_id; ++i) {
        transfer(parent_data, i, i + 1, i); // void * parent_data, local_id src, local_id dst, balance_t amount
    }
    if (max_id > 1) {
        transfer(parent_data, max_id, 1, 1); // second want send money to first
    }
}
