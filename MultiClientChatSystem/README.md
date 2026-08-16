# Multi-Client Chat System with Performance Analytics

## System Architecture Overview

This project implements a multi-client chat application consisting of three core components: a Chat Client, a Discovery Server, and a Chat Server. The system is capable of handling broadcast messages, private messaging, and user authentication with an active user list.

* **Discovery Server (`discovery_server`):** This acts as a DNS-like service. It listens for client registrations, stores their credentials, and assigns unique ports (though the current implementation uses a single well-known port for simplicity).
* **Chat Client (`chat_client`):** A command-line interface (CLI) application. It connects first to the Discovery Server to register or authenticate, and then to the Chat Server to send and receive messages. It utilizes `select()` for non-blocking I/O multiplexing to handle user input and incoming server messages simultaneously.
* **Chat Server:** The core routing component. It handles the connections and message passing. To benchmark performance, the Chat Server was implemented using three different concurrency models:
    * **Fork-based (`server_fork`):** Spawns a new child process for every client using `fork()`. IPC (Inter-Process Communication) is managed via named pipes (FIFOs) and shared memory (`mmap`).
    * **Thread-based (`server_thread`):** Spawns a new thread for every client using `pthreads`. State is shared directly in memory, protected by a mutex lock.
    * **Epoll-based (`server_epoll`):** A single-threaded, non-blocking implementation using the `epoll` API to efficiently monitor multiple file descriptors for events.

## Protocol Specification

The system uses a custom application-layer protocol running over TCP. Communication uses a Type-Length-Value (TLV) structure.

### Message Header
Every message begins with a fixed 3-byte header, defined as follows:

```c
typedef struct __attribute__((packed)) {
    uint8_t type;    // 1 byte: The command/message type
    uint16_t length; // 2 bytes: The length of the payload (Network Byte Order)
} MessageHeader;
```

### Message Types (uint8_t)

The protocol defines the following operation codes:

- **1 (MSG_REGISTER):** Client registers with Discovery Server.
- **2 (MSG_LOGIN):** Client authenticates with Chat Server.
- **3 (MSG_BROADCAST):** Client sends a message to all users.
- **4 (MSG_PRIVATE):** Client sends a message to a specific user.
- **5 (MSG_USER_LIST_REQ):** Client requests active users.
- **6 (MSG_USER_LIST_RES):** Server responds with active users.
- **7 (MSG_STATUS_UPDATE):** Client updates status (available/busy/away).
- **8 (MSG_HISTORY_REQ):** Client requests chat history.
- **9 (MSG_HISTORY_RES):** Server sends chat history.
- **200 (MSG_ACK):** Generic success response.
- **255 (MSG_ERROR):** Generic error response.

### Payload Formatting

Payloads are transmitted as strings. The payload length is defined in the header, and the content format varies by message type. For example:

- **MSG_REGISTER & MSG_LOGIN:** `<username>:<password>`
- **MSG_PRIVATE:** `<target_user> <message>`

## Compilation and Execution Instructions

### Compilation

The project includes a Makefile for compilation. To build all components, run:

```bash
make all
```

This will create the executables in the root directory: `discovery_server`, `chat_client`, `server_fork`, `server_thread`, `server_epoll`, and `benchmark_client`.

### Execution

Start the Discovery Server:

```bash
./discovery_server
```

Start the Chat Server: Choose one of the concurrency models.

```bash
./server_epoll  # or ./server_fork, or ./server_thread
```

Start a Chat Client: Run this in a new terminal window.

```bash
./chat_client
```

## Client Commands

Once connected to the chat server, the following commands are available:

- `/broadcast <msg>` - Send broadcast messages
- `/msg <user> <msg>` - Send private messages
- `/users` - View online users and status
- `/status <state>` - Change status (available/busy/away)
- `/history` - View your chat history
- `/quit` - Disconnect

## Testing Guide

The `scripts/` directory contains tools for performance benchmarking.

### Generating Metrics

**Stress Testing:** The `stress_test.sh` script automates the process of spawning clients, logging metrics, and shutting down the servers. It tests all three server architectures from 10 to 50 concurrent clients in increments of 10, with 3 repetitions per data point. Each data point gets a freshly started server, so no allocator state carries over between load levels.

```bash
cd scripts
./stress_test.sh
```

This script generates two files in the `logs/` directory:

- **metrics.csv:** `server_type, num_clients, rep, cpu, vmrss, vmsize, pss, conns`. CPU is a delta of `utime+stime` from `/proc/<pid>/stat` over a 3-second window, not `ps -o %cpu` (which averages over the process lifetime and reads 0.0 on an idle server). For the fork model, all columns are summed over the parent and its children. `conns` is the number of established client connections at sampling time — if it is below `num_clients`, that row is not measuring what it claims to.
- **delivery_times.csv:** `server_type, rtt_ms`, one row per request, timed with `CLOCK_MONOTONIC`.

### Generating Visualizations

The `plot_metrics.py` script requires matplotlib, pandas, and seaborn. It reads the CSV files and generates graphs in the `plots/` directory.

```bash
cd scripts
python3 plot_metrics.py
```

This will output the following charts:

- **delivery_time_distribution.png**
- **cpu_usage.png**
- **pss_usage.png**
- **vmrss_usage.png**
- **vmsize_usage.png**

## Results Summary

Median of 3 repetitions at 50 concurrent clients:

| | fork | thread | epoll |
|---|---|---|---|
| CPU | 15.3% | 26.6% | **13.3%** |
| Resident (VmRSS) | 6.5 MB | 16.1 MB | **1.1 MB** |
| Virtual (VmSize) | 134 MB | 3,603 MB | **3 MB** |
| Median RTT | **0.11 ms** | 0.14 ms | 0.23 ms |
| Resident per extra client | 108 KB | 323 KB | **~0 KB** |

`epoll` holds resident memory constant across the entire load range while the other two grow linearly. The threaded server costs more resident memory than the forked one — each thread carries its own 8 MB stack and its own 64 MB glibc malloc arena (72 MB of address space per client, almost all of it reservation rather than committed pages), whereas forked children share the parent's pages copy-on-write.

`epoll` is also the *slowest* per request despite using the fewest resources: it is single-threaded, so all clients serialise through one event loop while fork and thread spread across cores.

See `learnings.txt` for the full analysis, including the four bugs this benchmark exposed and the measurement errors that invalidated the first round of results.
