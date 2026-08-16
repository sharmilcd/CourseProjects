#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "network_utils.h"

static int send_all(int socket_fd, const void *buffer, size_t length) {
    size_t bytes_sent = 0;
    const char *ptr = (const char *)buffer;

    while (bytes_sent < length) {
        ssize_t result = send(socket_fd, ptr + bytes_sent, length - bytes_sent, 0);
        if (result <= 0) return -1; 
        bytes_sent += result;
    }
    return 0;
}

static int recv_all(int socket_fd, void *buffer, size_t length) {
    size_t bytes_received = 0;
    char *ptr = (char *)buffer;

    while (bytes_received < length) {
        ssize_t result = recv(socket_fd, ptr + bytes_received, length - bytes_received, 0);
        if (result <= 0) return -1; 
        bytes_received += result;
    }
    return 0;
}

int setup_server_socket(int port) {
    int server_fd;
    struct sockaddr_in server_addr;
    int opt = 1;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        return -1;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return -1;
    }

    /* A backlog of 10 is too small for the load test: 50 clients connect at
       once and the fork server needs a fork() per accept, so the queue
       overflows and the kernel silently drops SYNs. */
    if (listen(server_fd, SOMAXCONN) < 0) {
        perror("Listen failed");
        return -1;
    }

    return server_fd;
}

int connect_to_server(const char *ip, int port) {
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return -1;
    }

    return sock;
}

int send_message(int socket_fd, MessageType type, const char *payload) {
    MessageHeader header;
    header.type = type;

    uint16_t payload_len = (payload != NULL) ? strlen(payload) : 0;
    header.length = htons(payload_len); // Convert to Network Byte Order

    /* Header and payload must leave in a single write. Sending them as two
       separate send() calls put a ~40ms floor under every request/response:
       Nagle's algorithm holds the second small write until the peer ACKs the
       3-byte header, and the peer has nothing to send back so its delayed-ACK
       timer only fires ~40ms later. One write, one segment, no stall. */
    size_t total = sizeof(MessageHeader) + payload_len;
    char stack_buf[1024];
    char *buf = stack_buf;

    if (total > sizeof(stack_buf)) {
        buf = (char *)malloc(total);
        if (buf == NULL) return -1;
    }

    memcpy(buf, &header, sizeof(MessageHeader));
    if (payload_len > 0) {
        memcpy(buf + sizeof(MessageHeader), payload, payload_len);
    }

    int result = send_all(socket_fd, buf, total);
    if (buf != stack_buf) free(buf);
    return result;
}

int receive_message(int socket_fd, MessageHeader *header, char **payload) {
    if (recv_all(socket_fd, header, sizeof(MessageHeader)) < 0) {
        return -1; 
    }

    uint16_t payload_len = ntohs(header->length);
    header->length = payload_len; 

    if (payload_len > 0) {
        *payload = (char *)malloc(payload_len + 1); 
        if (*payload == NULL) {
            perror("Memory allocation failed");
            return -1;
        }
        
        if (recv_all(socket_fd, *payload, payload_len) < 0) {
            free(*payload);
            return -1;
        }
        (*payload)[payload_len] = '\0';
    } else {
        *payload = NULL;
    }

    return 0;
}
