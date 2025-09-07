#include "taxi.h"

static struct Driver drivers[MAX_DRIVERS];
static int num_drivers = 0;

static struct Driver *find_driver(pid_t pid) {
    for (int i = 0; i < num_drivers; i++) {
        if (drivers[i].pid == pid) {
            return &drivers[i];
        }
    }
    return NULL;
}

static void create_driver() {
    if (num_drivers >= MAX_DRIVERS) {
        printf("Error: Maximum number of drivers reached (%d)\n", MAX_DRIVERS);
        return;
    }

    int pipe_to_child[2];
    int pipe_from_child[2];
    if (pipe(pipe_to_child) == -1 || pipe(pipe_from_child) == -1) {
        perror("create_driver: pipe failed");
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("create_driver: fork failed");
        close(pipe_to_child[0]);
        close(pipe_to_child[1]);
        close(pipe_from_child[0]);
        close(pipe_from_child[1]);
        return;
    } else if (pid == 0) {
        close(pipe_to_child[1]);
        close(pipe_from_child[0]);
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        driver_loop(pipe_to_child[0], pipe_from_child[1]);
    } else {
        close(pipe_to_child[0]);
        close(pipe_from_child[1]);
        FILE *to = fdopen(pipe_to_child[1], "w");
        FILE *from = fdopen(pipe_from_child[0], "r");
        if (!to || !from) {
            perror("create_driver: fdopen failed");
            if (to)
                fclose(to);
            if (from)
                fclose(from);
            close(pipe_to_child[1]);
            close(pipe_from_child[0]);
            kill(pid, SIGTERM);
            waitpid(pid, NULL, 0);
            return;
        }
        drivers[num_drivers].pid = pid;
        drivers[num_drivers].to_driver = to;
        drivers[num_drivers].from_driver = from;
        num_drivers++;
        printf("Driver created with PID %d\n", pid);
    }
}

static void send_task(pid_t pid, int sec) {
    struct Driver *d = find_driver(pid);
    if (!d) {
        printf("Driver with PID %d not found\n", pid);
        return;
    }
    if (!d->to_driver || !d->from_driver) {
        printf("Error: Invalid pipe for driver %d\n", pid);
        return;
    }
    fprintf(d->to_driver, "send_task %d\n", sec);
    if (fflush(d->to_driver) == EOF) {
        printf("Error writing to driver %d\n", pid);
        return;
    }

    char buf[MAX_BUF];
    if (fgets(buf, MAX_BUF, d->from_driver) == NULL) {
        printf("Error reading response from driver %d\n", pid);
        return;
    }
    buf[strcspn(buf, "\n")] = '\0';
    printf("%s\n", buf);
}

static void get_status(pid_t pid) {
    struct Driver *d = find_driver(pid);
    if (!d) {
        printf("Driver with PID %d not found\n", pid);
        return;
    }
    if (!d->to_driver || !d->from_driver) {
        printf("Error: Invalid pipe for driver %d\n", pid);
        return;
    }
    fprintf(d->to_driver, "get_status\n");
    if (fflush(d->to_driver) == EOF) {
        printf("Error writing to driver %d\n", pid);
        return;
    }

    char buf[MAX_BUF];
    if (fgets(buf, MAX_BUF, d->from_driver) == NULL) {
        printf("Error reading response from driver %d\n", pid);
        return;
    }
    buf[strcspn(buf, "\n")] = '\0';
    printf("Driver %d: %s\n", pid, buf);
}

static void get_drivers() {
    if (num_drivers == 0) {
        printf("No drivers created\n");
        return;
    }
    for (int i = 0; i < num_drivers; i++) {
        get_status(drivers[i].pid);
    }
}

int main() {
    printf("Taxi CLI started. Commands: create_driver, send_task <pid> <sec>, "
           "get_status <pid>, get_drivers, exit\n");

    char input[MAX_BUF];
    while (1) {
        printf("> ");
        fflush(stdout);
        if (fgets(input, MAX_BUF, stdin) == NULL) {
            break;
        }
        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "create_driver") == 0) {
            create_driver();
        } else if (strncmp(input, "send_task ", 10) == 0) {
            pid_t pid;
            int sec;
            if (sscanf(input + 10, "%d %d", &pid, &sec) == 2 && sec > 0) {
                send_task(pid, sec);
            } else {
                printf("Usage: send_task <pid> <seconds>\n");
            }
        } else if (strncmp(input, "get_status ", 11) == 0) {
            pid_t pid;
            if (sscanf(input + 11, "%d", &pid) == 1) {
                get_status(pid);
            } else {
                printf("Usage: get_status <pid>\n");
            }
        } else if (strcmp(input, "get_drivers") == 0) {
            get_drivers();
        } else if (strcmp(input, "exit") == 0) {
            break;
        } else {
            printf("Unknown command\n");
        }
    }

    for (int i = 0; i < num_drivers; i++) {
        if (drivers[i].to_driver)
            fclose(drivers[i].to_driver);
        if (drivers[i].from_driver)
            fclose(drivers[i].from_driver);
        kill(drivers[i].pid, SIGTERM);
        waitpid(drivers[i].pid, NULL, 0);
    }
    printf("CLI exited\n");
    return 0;
}