#include "taxi.h"

void driver_loop(int in_pipe, int out_pipe) {
    FILE *in = fdopen(in_pipe, "r");
    FILE *out = fdopen(out_pipe, "w");
    if (!in || !out) {
        perror("driver: fdopen failed");
        exit(1);
    }

    DriverState state = AVAILABLE;
    time_t end_time = 0;

    while (1) {
        char buf[MAX_BUF];
        struct timeval tv;
        fd_set fds;
        int fd = fileno(in);
        int timeout_sec;

        if (state == AVAILABLE) {
            timeout_sec = -1;
        } else {
            time_t now = time(NULL);
            timeout_sec = (int)(end_time - now);
            if (timeout_sec <= 0) {
                state = AVAILABLE;
                timeout_sec = -1;
            }
        }

        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        if (timeout_sec == -1) {
            if (select(fd + 1, &fds, NULL, NULL, NULL) < 0) {
                if (errno == EINTR)
                    continue;
                perror("driver: select failed");
                break;
            }
        } else {
            tv.tv_sec = timeout_sec;
            tv.tv_usec = 0;
            int ret = select(fd + 1, &fds, NULL, NULL, &tv);
            if (ret == 0) {
                state = AVAILABLE;
                continue;
            } else if (ret < 0) {
                if (errno == EINTR)
                    continue;
                perror("driver: select failed");
                break;
            }
        }

        if (fgets(buf, MAX_BUF, in) == NULL) {
            break;
        }
        buf[strcspn(buf, "\n")] = '\0';

        if (strcmp(buf, "get_status") == 0) {
            fprintf(out, "%s\n", state == AVAILABLE ? "Available" : "Busy");
            fflush(out);
        } else if (strncmp(buf, "send_task ", 10) == 0) {
            int sec;
            if (sscanf(buf + 10, "%d", &sec) != 1 || sec <= 0) {
                fprintf(out, "Invalid seconds\n");
                fflush(out);
                continue;
            }
            if (state == AVAILABLE) {
                end_time = time(NULL) + sec;
                state = BUSY;
                fprintf(out, "OK\n");
                fflush(out);
            } else {
                fprintf(out, "Busy\n");
                fflush(out);
            }
        } else {
            fprintf(out, "Unknown command\n");
            fflush(out);
        }
    }

    fclose(in);
    fclose(out);
    exit(0);
}