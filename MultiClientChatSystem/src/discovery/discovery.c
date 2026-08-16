#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"
#include "network_utils.h"

#define DISCOVERY_PORT 8080
#define MAX_USERS 200

typedef struct {
    char username[32];
    char password[32];
    char ip_address[16];
    int port;
} UserRecord;

UserRecord user_database[MAX_USERS];
int user_count = 0;
int next_assigned_port = 8000; // Global auto-incrementing port base

void handle_client(int client_fd, struct sockaddr_in *client_addr) {
    MessageHeader header;
    char *payload = NULL;

    if (receive_message(client_fd, &header, &payload) < 0) {
        close(client_fd);
        return;
    }

    if (header.type == MSG_REGISTER && payload != NULL) {
        char username[32], password[32];

        if (sscanf(payload, "%31[^:]:%31s", username, password) == 2) {
            
            int user_idx = -1;
            for (int i = 0; i < user_count; i++) {
                if (strcmp(user_database[i].username, username) == 0) {
                    user_idx = i;
                    break;
                }
            }

            if (user_idx != -1) {
                if (strcmp(user_database[user_idx].password, password) == 0) {
                    char success_msg[64];
                    snprintf(success_msg, sizeof(success_msg), "Welcome back! Port: %d", user_database[user_idx].port);
                    send_message(client_fd, MSG_ACK, success_msg);
                    printf("Returning user logged in: %s\n", username);
                } else {
                    send_message(client_fd, MSG_ERROR, "Incorrect password for existing username.");
                    printf("Failed login attempt for: %s (Wrong PW)\n", username);
                }
            } else {
                if (user_count < MAX_USERS) {
                    strcpy(user_database[user_count].username, username);
                    strcpy(user_database[user_count].password, password);
                    strcpy(user_database[user_count].ip_address, inet_ntoa(client_addr->sin_addr));
                    
                    next_assigned_port++; // Increment port for uniqueness
                    user_database[user_count].port = next_assigned_port; 
                    
                    char success_msg[64];
                    snprintf(success_msg, sizeof(success_msg), "Registered! Assigned Port: %d", next_assigned_port);
                    send_message(client_fd, MSG_ACK, success_msg);
                    
                    printf("New User Registered: %s assigned to port %d\n", username, next_assigned_port);
                    user_count++;
                } else {
                    send_message(client_fd, MSG_ERROR, "Server database full");
                }
            }
        } else {
            send_message(client_fd, MSG_ERROR, "Invalid payload format");
        }
    }
    if (payload) free(payload);
    close(client_fd);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    printf("Starting Discovery Server on port %d...\n", DISCOVERY_PORT);

    server_fd = setup_server_socket(DISCOVERY_PORT);
    if (server_fd < 0) {
        exit(EXIT_FAILURE);
    }

    printf("Discovery Server listening for registrations...\n");

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }

        handle_client(client_fd, &client_addr);
    }

    close(server_fd);
    return 0;
}