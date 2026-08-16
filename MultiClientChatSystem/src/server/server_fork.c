#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include "protocol.h"
#include "network_utils.h"

#define CHAT_PORT 9000
#define MAX_CLIENTS 100

typedef struct {
    char username[32];
    char password[32];
    char status[16];
    int is_online;
    int is_registered;
} ActiveUser;

typedef struct {
    ActiveUser users[MAX_CLIENTS];
} SharedData;

SharedData *shared_mem;

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

void setup_shared_memory() {
    shared_mem = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, 
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared_mem == MAP_FAILED) exit(EXIT_FAILURE);
    for(int i=0; i<MAX_CLIENTS; i++) {
        shared_mem->users[i].is_online = 0;
        shared_mem->users[i].is_registered = 0;
        strcpy(shared_mem->users[i].status, "available");
    }
}

typedef struct {
    MessageType type;
    char sender[32];
    char text[256];
} InternalMessage;

void handle_client(int client_fd) {
    MessageHeader header;
    char *payload = NULL;
    char current_user[32] = "";
    int user_index = -1;

    if (receive_message(client_fd, &header, &payload) == 0 && header.type == MSG_LOGIN) {
        char pass[32];
        sscanf(payload, "%31[^:]:%31s", current_user, pass);
        
        int auth_failed = 0;
        char error_msg[64] = "";

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (shared_mem->users[i].is_registered && strcmp(shared_mem->users[i].username, current_user) == 0) {
                user_index = i;
                if (strcmp(shared_mem->users[i].password, pass) != 0) {
                    auth_failed = 1;
                    strcpy(error_msg, "Incorrect password.");
                } else if (shared_mem->users[i].is_online) {
                    auth_failed = 1;
                    strcpy(error_msg, "User already signed in elsewhere.");
                }
                break;
            }
        }

        if (auth_failed) {
            send_message(client_fd, MSG_ERROR, error_msg);
            exit(1); 
        } 
        else if (user_index != -1) {
            shared_mem->users[user_index].is_online = 1;
            send_message(client_fd, MSG_ACK, "Welcome back!");
        } 
        else {
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (!shared_mem->users[i].is_registered) {
                    user_index = i;
                    strcpy(shared_mem->users[i].username, current_user);
                    strcpy(shared_mem->users[i].password, pass);
                    shared_mem->users[i].is_online = 1;
                    shared_mem->users[i].is_registered = 1;
                    break;
                }
            }
            if (user_index != -1) {
                send_message(client_fd, MSG_ACK, "Registration & Login Successful");
            } else {
                send_message(client_fd, MSG_ERROR, "Server Full");
                exit(1);
            }
        }
    } else {
        send_message(client_fd, MSG_ERROR, "Auth Failed");
        exit(1);
    }
    if (payload) { free(payload); payload = NULL; }

    // 2. Setup IPC (Named Pipe / FIFO)
    char fifo_name[64];
    snprintf(fifo_name, sizeof(fifo_name), "/tmp/chat_%s", current_user);
    mkfifo(fifo_name, 0666);
    int fifo_fd = open(fifo_name, O_RDWR | O_NONBLOCK); 

    printf("[Child %d] %s joined. Listening on %s\n", getpid(), current_user, fifo_name);

    fd_set read_fds;
    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(client_fd, &read_fds);
        FD_SET(fifo_fd, &read_fds);
        int max_fd = (client_fd > fifo_fd) ? client_fd : fifo_fd;

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) break;

        if (FD_ISSET(fifo_fd, &read_fds)) {
            InternalMessage int_msg;
            if (read(fifo_fd, &int_msg, sizeof(InternalMessage)) > 0) {
                char out_buffer[512];
                if (int_msg.type == MSG_BROADCAST) {
                    snprintf(out_buffer, sizeof(out_buffer), "[Broadcast from %s]: %s", int_msg.sender, int_msg.text);
                } else {
                    snprintf(out_buffer, sizeof(out_buffer), "[Private from %s]: %s", int_msg.sender, int_msg.text);
                }
                send_message(client_fd, int_msg.type, out_buffer);
            }
        }

        if (FD_ISSET(client_fd, &read_fds)) {
            if (receive_message(client_fd, &header, &payload) < 0) break; 

            if (header.type == MSG_USER_LIST_REQ) {
                /* 512 bytes was not enough: each "- <user> [<status>]" line runs
                   to ~21 bytes, so once ~24 users were online the unbounded
                   strcat ran off the end of the buffer and smashed the stack.
                   Size it for the worst case and append with bounds checking. */
                char user_list[MAX_CLIENTS * 64 + 32];
                size_t used = snprintf(user_list, sizeof(user_list), "Online Users:\n");
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (shared_mem->users[i].is_online && used < sizeof(user_list) - 1) {
                        used += snprintf(user_list + used, sizeof(user_list) - used,
                                         "- %s [%s]\n", shared_mem->users[i].username,
                                         shared_mem->users[i].status);
                    }
                }
                send_message(client_fd, MSG_USER_LIST_RES, user_list);
            } 
            else if (header.type == MSG_BROADCAST && payload != NULL) {
                InternalMessage int_msg;
                int_msg.type = MSG_BROADCAST;
                strcpy(int_msg.sender, current_user);
                strncpy(int_msg.text, payload, 255);

                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (shared_mem->users[i].is_online && i != user_index) {
                        char target_fifo[64];
                        snprintf(target_fifo, sizeof(target_fifo), "/tmp/chat_%s", shared_mem->users[i].username);
                        int t_fd = open(target_fifo, O_WRONLY | O_NONBLOCK);
                        if (t_fd >= 0) {
                            write(t_fd, &int_msg, sizeof(InternalMessage));
                            close(t_fd);
                            save_chat_history(shared_mem->users[i].username, current_user, "broadcast", payload);
                        }
                    }
                }
                save_chat_history(current_user, current_user, "broadcast_sent", payload);
            }
            else if (header.type == MSG_PRIVATE && payload != NULL) {
                char target[32], msg_text[256];
                if (sscanf(payload, "%31s %[^\n]", target, msg_text) == 2) {
                    InternalMessage int_msg;
                    int_msg.type = MSG_PRIVATE;
                    strcpy(int_msg.sender, current_user);
                    strcpy(int_msg.text, msg_text);

                    char target_fifo[64];
                    snprintf(target_fifo, sizeof(target_fifo), "/tmp/chat_%s", target);
                    int t_fd = open(target_fifo, O_WRONLY | O_NONBLOCK);
                    if (t_fd >= 0) {
                        write(t_fd, &int_msg, sizeof(InternalMessage));
                        close(t_fd);
                        save_chat_history(target, current_user, "private", msg_text);
                        save_chat_history(current_user, current_user, "private_sent", msg_text);
                    } else {
                        send_message(client_fd, MSG_ERROR, "User not found or offline");
                    }
                }
            }
            else if (header.type == MSG_STATUS_UPDATE && payload != NULL) {
                if (strcmp(payload, "available") == 0 || strcmp(payload, "busy") == 0 || strcmp(payload, "away") == 0) {
                    strncpy(shared_mem->users[user_index].status, payload, 15);
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
    }

    // 4. Cleanup on Disconnect
    if (user_index != -1) shared_mem->users[user_index].is_online = 0; // Mark offline
    close(fifo_fd);
    unlink(fifo_name);
    close(client_fd);
    printf("[Child %d] %s disconnected.\n", getpid(), current_user);
    exit(0);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    mkdir("../data", 0777);
    setup_shared_memory();
    if ((server_fd = setup_server_socket(CHAT_PORT)) < 0) exit(EXIT_FAILURE);

    printf("Forked Chat Server listening on port %d...\n", CHAT_PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) continue;

        while (waitpid(-1, NULL, WNOHANG) > 0); 

        pid_t pid = fork();
        if (pid == 0) {
            close(server_fd);
            handle_client(client_fd);
        } else if (pid > 0) {
            close(client_fd);
        }
    }
    return 0;
}
