#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/stat.h>
#include "protocol.h"
#include "network_utils.h"

#define CHAT_PORT 9000
#define MAX_CLIENTS 100

typedef struct {
    char username[32];
    char password[32];
    char status[16]; 
    int socket_fd;
    int is_online;
    int is_registered;
} ActiveUser;

ActiveUser users[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void save_chat_history(const char* target_user, const char* sender, const char* msg_type, const char* message) {
    char filepath[128];
    snprintf(filepath, sizeof(filepath), "../data/history_%s.json", target_user);
    
    FILE *f = fopen(filepath, "a");
    if (!f) return;
    
    time_t now = time(NULL);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    fprintf(f, "{\"timestamp\": \"%s\", \"sender\": \"%s\", \"type\": \"%s\", \"message\": \"%s\"}\n",
            time_str, sender, msg_type, message);
    fclose(f);
}

void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    MessageHeader header;
    char *payload = NULL;
    char current_user[32] = "";
    int user_index = -1;

    if (receive_message(client_fd, &header, &payload) == 0 && header.type == MSG_LOGIN) {
        char pass[32];
        sscanf(payload, "%31[^:]:%31s", current_user, pass);
        
        int auth_failed = 0;
        char error_msg[64] = "";

        pthread_mutex_lock(&clients_mutex);
        
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (users[i].is_registered && strcmp(users[i].username, current_user) == 0) {
                user_index = i;
                if (strcmp(users[i].password, pass) != 0) {
                    auth_failed = 1;
                    strcpy(error_msg, "Incorrect password.");
                } else if (users[i].is_online) {
                    auth_failed = 1;
                    strcpy(error_msg, "User already signed in elsewhere.");
                }
                break;
            }
        }

        if (auth_failed) {
            pthread_mutex_unlock(&clients_mutex);
            send_message(client_fd, MSG_ERROR, error_msg);
            close(client_fd);
            if (payload) free(payload);
            pthread_exit(NULL);
        } 
        else if (user_index != -1) {
            users[user_index].socket_fd = client_fd;
            users[user_index].is_online = 1;
            pthread_mutex_unlock(&clients_mutex);
            send_message(client_fd, MSG_ACK, "Welcome back!");
            printf("[Thread %lu] %s logged back in.\n", pthread_self(), current_user);
        } 
        else {
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (!users[i].is_registered) {
                    user_index = i;
                    strcpy(users[i].username, current_user);
                    strcpy(users[i].password, pass);
                    users[i].socket_fd = client_fd;
                    users[i].is_online = 1;
                    users[i].is_registered = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            
            if (user_index != -1) {
                send_message(client_fd, MSG_ACK, "Registration & Login Successful");
                printf("[Thread %lu] %s joined for the first time.\n", pthread_self(), current_user);
            } else {
                send_message(client_fd, MSG_ERROR, "Server Full");
                close(client_fd);
                if (payload) free(payload);
                pthread_exit(NULL);
            }
        }
    } else {
        send_message(client_fd, MSG_ERROR, "Auth Failed");
        close(client_fd);
        if (payload) free(payload);
        pthread_exit(NULL);
    }
    if (payload) { free(payload); payload = NULL; }

    while (receive_message(client_fd, &header, &payload) == 0) {
        if (header.type == MSG_USER_LIST_REQ) {
            /* 512 bytes was not enough: each "- <user> [<status>]" line runs to
               ~21 bytes, so once ~24 users were online the unbounded strcat ran
               off the end of the buffer and smashed the stack. Size it for the
               worst case and append with bounds checking. */
            char user_list[MAX_CLIENTS * 64 + 32];
            size_t used = snprintf(user_list, sizeof(user_list), "Online Users:\n");
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (users[i].is_online && used < sizeof(user_list) - 1) {
                    used += snprintf(user_list + used, sizeof(user_list) - used,
                                     "- %s [%s]\n", users[i].username, users[i].status);
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            send_message(client_fd, MSG_USER_LIST_RES, user_list);
        } 
        else if (header.type == MSG_BROADCAST && payload != NULL) {
            char out_buffer[512];
            snprintf(out_buffer, sizeof(out_buffer), "[Broadcast from %s]: %s", current_user, payload);

            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (users[i].is_online && i != user_index) {
                    send_message(users[i].socket_fd, MSG_BROADCAST, out_buffer);
                    save_chat_history(users[i].username, current_user, "broadcast", payload);
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            save_chat_history(current_user, current_user, "broadcast_sent", payload);
        }
        else if (header.type == MSG_PRIVATE && payload != NULL) {
            char target[32], msg_text[256];
            if (sscanf(payload, "%31s %[^\n]", target, msg_text) == 2) {
                char out_buffer[512];
                snprintf(out_buffer, sizeof(out_buffer), "[Private from %s]: %s", current_user, msg_text);
                
                int target_found = 0;
                pthread_mutex_lock(&clients_mutex);
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (users[i].is_online && strcmp(users[i].username, target) == 0) {
                        send_message(users[i].socket_fd, MSG_PRIVATE, out_buffer);
                        target_found = 1;
                        break;
                    }
                }
                pthread_mutex_unlock(&clients_mutex);

                if (target_found) {
                    save_chat_history(target, current_user, "private", msg_text);
                    save_chat_history(current_user, current_user, "private_sent", msg_text);
                } else {
                    send_message(client_fd, MSG_ERROR, "User not found or offline");
                }
            }
        }
        else if (header.type == MSG_STATUS_UPDATE && payload != NULL) {
            if (strcmp(payload, "available") == 0 || strcmp(payload, "busy") == 0 || strcmp(payload, "away") == 0) {
                pthread_mutex_lock(&clients_mutex);
                strncpy(users[user_index].status, payload, 15);
                pthread_mutex_unlock(&clients_mutex);
                send_message(client_fd, MSG_ACK, "Status updated.");
            } else {
                send_message(client_fd, MSG_ERROR, "Invalid status. Use 'available', 'busy', or 'away'.");
            }
        }
        else if (header.type == MSG_HISTORY_REQ) {
            char filepath[128];
            snprintf(filepath, sizeof(filepath), "../data/history_%s.json", current_user);
            FILE *f = fopen(filepath, "r");
            if (f) {
                char hist_buf[8192] = "";
                char line[256];
                while (fgets(line, sizeof(line), f)) {
                    if (strlen(hist_buf) + strlen(line) < sizeof(hist_buf) - 1) strcat(hist_buf, line);
                    else { strcat(hist_buf, "... [Truncated] ..."); break; }
                }
                fclose(f);
                send_message(client_fd, MSG_HISTORY_RES, hist_buf);
            } else {
                send_message(client_fd, MSG_HISTORY_RES, "No chat history found.");
            }
        }
        
        if (payload) { free(payload); payload = NULL; }
    }

    pthread_mutex_lock(&clients_mutex);
    users[user_index].is_online = 0; 
    pthread_mutex_unlock(&clients_mutex);
    
    close(client_fd);
    printf("[Thread %lu] %s disconnected.\n", pthread_self(), current_user);
    pthread_exit(NULL);
}

int main() {
    int server_fd;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    mkdir("../data", 0777);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        users[i].is_online = 0;
        users[i].is_registered = 0;
        strcpy(users[i].status, "available");
    }

    if ((server_fd = setup_server_socket(CHAT_PORT)) < 0) exit(EXIT_FAILURE);
    printf("Threaded Chat Server listening on port %d...\n", CHAT_PORT);

    while (1) {
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (*client_fd < 0) {
            free(client_fd);
            continue;
        }

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, (void *)client_fd) != 0) {
            close(*client_fd);
            free(client_fd);
            continue;
        }
        pthread_detach(tid);
    }
    return 0;
}
