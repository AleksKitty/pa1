//
// Created by alex on 20.06.2020.
//

#include <stdio.h>
#include <stdarg.h>

#include "log.h"

static void log (pid_t p, const char * f, const char * m, ...){
    va_list args;
    va_start(args, m); // for reading arg
    printf("p:%d\t\tf:%s\t\tm:", p, f);
    vprintf(m, args);
    printf("\n");
    va_end(args);
}
