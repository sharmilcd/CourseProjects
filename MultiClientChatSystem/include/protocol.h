#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

typedef enum
{
    MSG_REGISTER = 1,      // Client -> Discovery Server
    MSG_LOGIN = 2,         // Client -> Chat Server
    MSG_BROADCAST = 3,     // Client -> Chat Server -> All Clients
    MSG_PRIVATE = 4,       // Client -> Chat Server -> Specific Client
    MSG_USER_LIST_REQ = 5, // Client -> Chat Server
    MSG_USER_LIST_RES = 6, // Chat Server -> Client
    MSG_STATUS_UPDATE = 7, // Client -> Chat Server (Bonus)
    MSG_HISTORY_REQ = 8,   // Client -> Chat Server (Bonus)
    MSG_HISTORY_RES    = 9, // Server -> Client History Transfer
    MSG_ACK = 200,         // Generic Success
    MSG_ERROR = 255        // Generic Error
} MessageType;

#if defined(_MSC_VER)
#pragma pack(push, 1)
typedef struct
{
    uint8_t type;    // 1 byte: What kind of message is this?
    uint16_t length; // 2 bytes: How long is the payload coming next?
} MessageHeader;
#pragma pack(pop)
#else
typedef struct __attribute__((packed))
{
    uint8_t type;    // 1 byte: What kind of message is this?
    uint16_t length; // 2 bytes: How long is the payload coming next?
} MessageHeader;
#endif


#endif // PROTOCOL_H
