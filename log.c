//
// Created by alex on 20.06.2020.
//

#include <stdio.h>
#include <stdarg.h>

#include "log.h"

void lg (pid_t p, const char * f, const char * m, ...){
    va_list args;
    va_start(args, m); // for reading arg
    printf("p:%d\t\tf:%-14s\t\tm:", p, f);
    vprintf(m, args);
    printf("\n");
    va_end(args);
}
