#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include <stdint.h>
#include <sys/types.h>
#include "protocol.h"

int setup_server_socket(int port);
int connect_to_server(const char *ip, int port);

int send_message(int socket_fd, MessageType type, const char *payload);
int receive_message(int socket_fd, MessageHeader *header, char **payload);

#endif 
