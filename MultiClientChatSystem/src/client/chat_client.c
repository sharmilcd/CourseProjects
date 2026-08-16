#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include "protocol.h"
#include "network_utils.h"

#define DISCOVERY_IP "127.0.0.1"
#define DISCOVERY_PORT 8080
#define CHAT_SERVER_IP "127.0.0.1"
#define CHAT_SERVER_PORT 9000

void run_cli_loop(int chat_fd) {
    fd_set read_fds;
    char input_buf[256];
    MessageHeader header;
    char *payload = NULL;

    printf("\n=== Chat CLI Started ===\n");
    printf("Commands:\n");
    printf("  /broadcast <msg>      - Send broadcast messages\n");
    printf("  /msg <user> <msg>     - Send private messages\n");
    printf("  /users                - View online users and status\n");
    printf("  /status <state>       - Change status (available/busy/away)\n"); 
    printf("  /history              - View your chat history\n");            
    printf("  /quit                 - Disconnect\n");
    printf("> ");
    fflush(stdout);

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds); 
        FD_SET(chat_fd, &read_fds);     

        int max_fd = (STDIN_FILENO > chat_fd) ? STDIN_FILENO : chat_fd;

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("select error");
            break;
        }

        if (FD_ISSET(chat_fd, &read_fds)) {
            if (receive_message(chat_fd, &header, &payload) < 0) {
                printf("\n[Disconnected from server]\n");
                break;
            }
            
            if (header.type == MSG_HISTORY_RES) {
                printf("\n--- Your Chat History ---\n%s\n-------------------------\n> ", 
                       payload ? payload : "No history.");
            } else {
                printf("\n[Incoming] %s\n> ", payload ? payload : "");
            }
            
            fflush(stdout);
            
            if (payload) { 
                free(payload); 
                payload = NULL; 
            }
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (fgets(input_buf, sizeof(input_buf), stdin) != NULL) {
                input_buf[strcspn(input_buf, "\n")] = 0; 

                if (strncmp(input_buf, "/quit", 5) == 0) {
                    break;
                } else if (strncmp(input_buf, "/broadcast ", 11) == 0) {
                    send_message(chat_fd, MSG_BROADCAST, input_buf + 11);
                } else if (strncmp(input_buf, "/users", 6) == 0) {
                    send_message(chat_fd, MSG_USER_LIST_REQ, "");
                } else if (strncmp(input_buf, "/msg ", 5) == 0) {
                    send_message(chat_fd, MSG_PRIVATE, input_buf + 5);
                } else if (strncmp(input_buf, "/status ", 8) == 0) {
                    send_message(chat_fd, MSG_STATUS_UPDATE, input_buf + 8);
                } else if (strncmp(input_buf, "/history", 8) == 0) {
                    send_message(chat_fd, MSG_HISTORY_REQ, "");
                } else if (strlen(input_buf) > 0) {
                    printf("Unknown command. Use /broadcast, /msg, /users, /status, or /history.\n");
                }
                
                printf("> ");
                fflush(stdout);
            }
        }
    }
}

int main() {
    char username[32];
    char password[32];
    int client_port; 

    printf("=== Welcome to the Chat System ===\n");
    
    printf("Enter Username: ");
    scanf("%31s", username);
    printf("Enter Password: ");
    scanf("%31s", password);

    int c;
    while ((c = getchar()) != '\n' && c != EOF); 

    printf("\nRegistering with Discovery Server...\n");
    int discovery_fd = connect_to_server(DISCOVERY_IP, DISCOVERY_PORT);
    if (discovery_fd < 0) exit(EXIT_FAILURE);

    char payload[100];
    snprintf(payload, sizeof(payload), "%s:%s", username, password);
    send_message(discovery_fd, MSG_REGISTER, payload);

    MessageHeader header;
    char *response_payload = NULL;
    receive_message(discovery_fd, &header, &response_payload);
    
    if (header.type != MSG_ACK) {
        printf("[ERROR] Registration failed: %s\n", response_payload ? response_payload : "Unknown");
        if (response_payload) free(response_payload);
        close(discovery_fd);
        exit(EXIT_FAILURE);
    }
    
    if (response_payload) free(response_payload);
    close(discovery_fd); 

    printf("Connecting to Chat Server at %s:%d...\n", CHAT_SERVER_IP, CHAT_SERVER_PORT);
    int chat_fd = connect_to_server(CHAT_SERVER_IP, CHAT_SERVER_PORT);
    if (chat_fd < 0) {
        printf("Failed to connect to Chat Server. Exiting.\n");
        exit(EXIT_FAILURE);
    }

    snprintf(payload, sizeof(payload), "%s:%s", username, password);
    if (send_message(chat_fd, MSG_LOGIN, payload) < 0) {
        perror("Failed to send login message");
        close(chat_fd);
        exit(EXIT_FAILURE);
    }

    response_payload = NULL;
    if (receive_message(chat_fd, &header, &response_payload) < 0 || header.type != MSG_ACK) {
        printf("[ERROR] Login failed: %s\n", response_payload ? response_payload : "Invalid credentials");
        if (response_payload) free(response_payload);
        close(chat_fd);
        exit(EXIT_FAILURE);
    }
    if (response_payload) free(response_payload);

    run_cli_loop(chat_fd);

    printf("Exiting chat client.\n");
    close(chat_fd);
    return 0;
}
