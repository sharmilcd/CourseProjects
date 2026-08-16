#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>       
#include <sys/stat.h>   
#include "protocol.h"
#include "network_utils.h"

#define CHAT_PORT 9000 
#define MAX_CLIENTS 50
#define MAX_EVENTS 10

typedef struct {
    char username[32];
    char password[32]; 
    char status[16];   
    int socket_fd;
    int is_online;
    int is_registered; 
} ActiveUser;

ActiveUser users[MAX_CLIENTS];

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return;
    }
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

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

int main() {
    int server_fd, epoll_fd;
    struct epoll_event event, events[MAX_EVENTS];

    mkdir("../data", 0777);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        users[i].is_online = 0;
        users[i].is_registered = 0;
        strcpy(users[i].status, "available"); // NEW: Default status
    }

    if ((server_fd = setup_server_socket(CHAT_PORT)) < 0) exit(EXIT_FAILURE);
    
    set_nonblocking(server_fd);

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    event.events = EPOLLIN;
    event.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        perror("epoll_ctl: server_fd");
        exit(EXIT_FAILURE);
    }

    printf("Epoll (Non-blocking) Chat Server listening on port %d...\n", CHAT_PORT);

    while (1) {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_events == -1) {
            perror("epoll_wait");
            break;
        }

        for (int n = 0; n < num_events; n++) {
            int active_fd = events[n].data.fd;

            if (active_fd == server_fd) {
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
                
                if (client_fd == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        continue;
                    }
                    perror("accept");
                    continue;
                }

                printf("New connection accepted (FD: %d)\n", client_fd);
                
                event.events = EPOLLIN;
                event.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);
            } 
            else {
                MessageHeader header;
                char *payload = NULL;

                if (receive_message(active_fd, &header, &payload) < 0) {
                    printf("Client disconnected (FD: %d)\n", active_fd);
                    
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (users[i].is_online && users[i].socket_fd == active_fd) {
                            users[i].is_online = 0;
                            break;
                        }
                    }
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, active_fd, NULL);
                    close(active_fd);
                } 
                else {
                    
                    if (header.type == MSG_LOGIN && payload != NULL) {
                        char current_user[32], pass[32];
                        sscanf(payload, "%31[^:]:%31s", current_user, pass);
                        
                        int user_index = -1;
                        int auth_failed = 0;
                        char error_msg[64] = "";

                        for (int i = 0; i < MAX_CLIENTS; i++) {
                            if (users[i].is_registered && strcmp(users[i].username, current_user) == 0) {
                                user_index = i;
                                
                                if (strcmp(users[i].password, pass) != 0) {
                                    auth_failed = 1;
                                    strcpy(error_msg, "Incorrect password.");
                                } 
                                else if (users[i].is_online) {
                                    auth_failed = 1;
                                    strcpy(error_msg, "User is already signed in from another location.");
                                }
                                break;
                            }
                        }

                        if (auth_failed) {
                            send_message(active_fd, MSG_ERROR, error_msg);
                        } 
                        else if (user_index != -1) {
                            users[user_index].socket_fd = active_fd;
                            users[user_index].is_online = 1;
                            send_message(active_fd, MSG_ACK, "Login Successful");
                            printf("%s logged back in.\n", current_user);
                        } 
                        else {
                            for (int i = 0; i < MAX_CLIENTS; i++) {
                                if (!users[i].is_registered) {
                                    user_index = i;
                                    strcpy(users[i].username, current_user);
                                    strcpy(users[i].password, pass);
                                    users[i].socket_fd = active_fd;
                                    users[i].is_online = 1;
                                    users[i].is_registered = 1; 
                                    strcpy(users[i].status, "available"); 
                                    break;
                                }
                            }
                            
                            if (user_index != -1) {
                                send_message(active_fd, MSG_ACK, "Login Successful");
                                printf("%s joined for the first time.\n", current_user);
                            } else {
                                send_message(active_fd, MSG_ERROR, "Server Full");
                            }
                        }
                    }
                    else if (header.type == MSG_BROADCAST && payload != NULL) {
                        char sender[32] = "Unknown";
                        for (int i = 0; i < MAX_CLIENTS; i++) {
                            if (users[i].is_online && users[i].socket_fd == active_fd) {
                                strcpy(sender, users[i].username);
                                break;
                            }
                        }

                        char out_buffer[512];
                        snprintf(out_buffer, sizeof(out_buffer), "[Broadcast from %s]: %s", sender, payload);

                        for (int i = 0; i < MAX_CLIENTS; i++) {
                            if (users[i].is_online && users[i].socket_fd != active_fd) {
                                send_message(users[i].socket_fd, MSG_BROADCAST, out_buffer);
                                save_chat_history(users[i].username, sender, "broadcast", payload);
                            }
                        }
                    }
                    else if (header.type == MSG_PRIVATE && payload != NULL) {
                        char sender[32] = "Unknown";
                        for (int i = 0; i < MAX_CLIENTS; i++) {
                            if (users[i].is_online && users[i].socket_fd == active_fd) {
                                strcpy(sender, users[i].username);
                                break;
                            }
                        }

                        char target[32], msg_text[256];
                        if (sscanf(payload, "%31s %[^\n]", target, msg_text) == 2) {
                            char out_buffer[512];
                            snprintf(out_buffer, sizeof(out_buffer), "[Private from %s]: %s", sender, msg_text);
                            
                            int target_found = 0;
                            for (int i = 0; i < MAX_CLIENTS; i++) {
                                if (users[i].is_online && strcmp(users[i].username, target) == 0) {
                                    send_message(users[i].socket_fd, MSG_PRIVATE, out_buffer);
                                    save_chat_history(target, sender, "private", msg_text);
                                    save_chat_history(sender, sender, "private_sent", msg_text);
                                    target_found = 1;
                                    break;
                                }
                            }
                            if (!target_found) {
                                send_message(active_fd, MSG_ERROR, "User not found or offline");
                            }
                        }
                    }
                    else if (header.type == MSG_USER_LIST_REQ) {
                        /* 512 bytes was not enough: each "- <user> [<status>]"
                           line runs to ~21 bytes, so once ~24 users were online
                           the unbounded strcat ran off the end of the buffer and
                           smashed the stack. Size it for the worst case and
                           append with bounds checking. */
                        char user_list[MAX_CLIENTS * 64 + 32];
                        size_t used = snprintf(user_list, sizeof(user_list), "Online Users:\n");
                        for (int i = 0; i < MAX_CLIENTS; i++) {
                            if (users[i].is_online && used < sizeof(user_list) - 1) {
                                used += snprintf(user_list + used, sizeof(user_list) - used,
                                                 "- %s [%s]\n", users[i].username, users[i].status);
                            }
                        }
                        send_message(active_fd, MSG_USER_LIST_RES, user_list);
                    }
                    else if (header.type == MSG_STATUS_UPDATE && payload != NULL) {
                        if (strcmp(payload, "available") == 0 || strcmp(payload, "busy") == 0 || strcmp(payload, "away") == 0) {
                            for (int i = 0; i < MAX_CLIENTS; i++) {
                                if (users[i].is_online && users[i].socket_fd == active_fd) {
                                    strncpy(users[i].status, payload, 15);
                                    send_message(active_fd, MSG_ACK, "Status updated successfully.");
                                    break;
                                }
                            }
                        } else {
                            send_message(active_fd, MSG_ERROR, "Invalid status. Use 'available', 'busy', or 'away'.");
                        }
                    }
                    else if (header.type == MSG_HISTORY_REQ) {
                        char username[32] = "";
                        for (int i = 0; i < MAX_CLIENTS; i++) {
                            if (users[i].is_online && users[i].socket_fd == active_fd) {
                                strcpy(username, users[i].username);
                                break;
                            }
                        }
                        
                        char filepath[128];
                        snprintf(filepath, sizeof(filepath), "../data/history_%s.json", username);
                        FILE *f = fopen(filepath, "r");
                        if (f) {
                            char hist_buf[8192] = ""; 
                            char line[256];
                            while (fgets(line, sizeof(line), f)) {
                                if (strlen(hist_buf) + strlen(line) < sizeof(hist_buf) - 1) {
                                    strcat(hist_buf, line);
                                } else {
                                    strcat(hist_buf, "... [History truncated] ...");
                                    break;
                                }
                            }
                            fclose(f);
                            send_message(active_fd, MSG_HISTORY_RES, hist_buf);
                        } else {
                            send_message(active_fd, MSG_HISTORY_RES, "No chat history found.");
                        }
                    }
                    
                    if (payload) { free(payload); payload = NULL; }
                }
            }
        }
    }

    close(server_fd);
    return 0;
}
