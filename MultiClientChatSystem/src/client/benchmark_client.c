#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "protocol.h"
#include "network_utils.h"

#define DISCOVERY_IP "127.0.0.1"
#define DISCOVERY_PORT 8080
#define CHAT_SERVER_IP "127.0.0.1"
#define CHAT_SERVER_PORT 9000

/* The client used to time a single request and then sleep(20). That left the
   server completely idle for the whole sampling window, so every CPU reading
   came back 0.0, and the one RTT sample per client was taken during the
   connect stampede rather than in steady state. It now drives a paced request
   stream for the duration of the run instead. */
#define RUN_SECONDS 25
#define REQUEST_GAP_US 10000   /* 10ms between requests -> ~100 req/s/client */

/* CLOCK_MONOTONIC, not gettimeofday(). gettimeofday reads CLOCK_REALTIME, which
   is free to jump backwards when the clock is corrected -- under WSL2 the VM
   resyncs against the host and that produced negative round-trip times in the
   logs. A monotonic clock cannot run backwards, which is what a stopwatch needs. */
double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000.0) + (ts.tv_nsec / 1000000.0);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <username> <password> <server_type_name>\n", argv[0]);
        return 1;
    }

    char *username = argv[1];
    char *password = argv[2];
    char *server_type = argv[3];

    int discovery_fd = connect_to_server(DISCOVERY_IP, DISCOVERY_PORT);
    if (discovery_fd < 0) return 1;

    char payload[100];
    snprintf(payload, sizeof(payload), "%s:%s", username, password);
    send_message(discovery_fd, MSG_REGISTER, payload);

    MessageHeader header;
    char *resp = NULL;
    receive_message(discovery_fd, &header, &resp);
    if (resp) free(resp);
    close(discovery_fd);

    int chat_fd = connect_to_server(CHAT_SERVER_IP, CHAT_SERVER_PORT);
    if (chat_fd < 0) return 1;

    snprintf(payload, sizeof(payload), "%s:%s", username, password);
    send_message(chat_fd, MSG_LOGIN, payload);
    if (receive_message(chat_fd, &header, &resp) < 0) {
        close(chat_fd);
        return 1;
    }
    if (header.type == MSG_ERROR) {          /* login rejected -- do not log RTTs */
        if (resp) free(resp);
        close(chat_fd);
        return 1;
    }
    if (resp) { free(resp); resp = NULL; }

    /* Held open for the whole run so 50 clients are not reopening the same
       file thousands of times; flushed per sample so a kill still leaves the
       completed measurements on disk. */
    FILE *log = fopen("../logs/delivery_times.csv", "a");

    double deadline = get_time_ms() + (RUN_SECONDS * 1000.0);

    while (get_time_ms() < deadline) {
        double start_time = get_time_ms();

        if (send_message(chat_fd, MSG_USER_LIST_REQ, "") < 0) break;

        int got_response = 0;
        while (!got_response) {
            if (receive_message(chat_fd, &header, &resp) < 0) goto done;

            if (header.type == MSG_USER_LIST_RES) {
                double rtt = get_time_ms() - start_time;
                if (log) {
                    fprintf(log, "%s,%f\n", server_type, rtt);
                    fflush(log);
                }
                got_response = 1;
            }
            if (resp) { free(resp); resp = NULL; }
        }

        usleep(REQUEST_GAP_US);
    }

done:
    if (resp) free(resp);
    if (log) fclose(log);
    close(chat_fd);
    return 0;
}
