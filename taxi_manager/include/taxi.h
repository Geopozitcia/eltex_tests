#ifndef TAXI_H
#define TAXI_H

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_DRIVERS 10
#define MAX_BUF 256

typedef enum { AVAILABLE, BUSY } DriverState;

struct Driver {
    pid_t pid;
    FILE *to_driver;   // Pipe для отправки команд
    FILE *from_driver; // Pipe для получения ответов
};

void driver_loop(int in_pipe, int out_pipe);

#endif