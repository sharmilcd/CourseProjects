# Performance Analysis Report: Multi-Client Chat System

> Supersedes `performance report.docx`, which was written from a benchmark run whose
> harness and code contained the defects listed in section 5. Several of its
> conclusions do not survive corrected measurement; the differences are noted in
> section 6.

## 1. Methodology

Three server architectures implement the same protocol and feature set and are
compared directly: fork-based (process per client), thread-based (pthread per
client), and non-blocking single-threaded `epoll`.

**Load.** Each simulated client registers with the discovery server, logs in to the
chat server, and then issues a paced stream of user-list requests (one per 10 ms)
for 25 seconds while holding its connection open. Load is stepped from 10 to 50
concurrent clients in increments of 10, with **3 repetitions per data point**;
every figure below is the median of those repetitions.

**Isolation.** Each data point starts a fresh server process. Reusing one server
across load levels contaminates the threaded model, because glibc caches exited
threads' stacks and does not return secondary arenas — the 50-client sample would
otherwise carry everything the earlier rounds left behind.

**Server-side metrics**, sampled while all N connections are live:

| Metric | Source | Note |
|---|---|---|
| CPU % | delta of `utime+stime` from `/proc/<pid>/stat` over a 3 s window | not `ps -o %cpu`, which averages over process lifetime and reads 0.0 on a mostly-blocked server |
| VmRSS | `/proc/<pid>/status` | resident (committed) pages |
| VmSize | `/proc/<pid>/status` | virtual address space, including pure reservation |
| PSS | `/proc/<pid>/smaps` | shared pages divided across the processes mapping them |
| conns | socket fds held | audit column: if `conns < num_clients`, that row is invalid |

For the fork model all columns are summed over the parent and its live children.

**Client-side metric.** Round-trip time per request, timed with `CLOCK_MONOTONIC`.

**Environment.** Ubuntu 24.04 on WSL2 (kernel 5.15, glibc 2.39), 20 logical cores,
loopback interface only, so no network latency is included.

## 2. Results

Median of 3 repetitions, 50 concurrent clients:

| | fork | thread | epoll |
|---|---|---|---|
| CPU | 15.3 % | 26.6 % | **13.3 %** |
| Resident (VmRSS) | 6.5 MB | 16.1 MB | **1.1 MB** |
| Virtual (VmSize) | 134 MB | 3,603 MB | **3 MB** |
| PSS | 3.0 MB | 15.4 MB | **0.14 MB** |
| Median RTT | **0.112 ms** | 0.139 ms | 0.231 ms |
| p99 RTT | **0.617 ms** | 0.964 ms | 1.412 ms |

Marginal cost of one additional client:

| | fork | thread | epoll |
|---|---|---|---|
| Resident | 108 KB | 323 KB | **~0 KB** |
| Virtual | 2.6 MB | 72.0 MB | **0 MB** |

CPU scaling (median %, by client count):

| | 10 | 20 | 30 | 40 | 50 |
|---|---|---|---|---|---|
| fork | 3.96 | 8.17 | 9.75 | 14.47 | 15.27 |
| thread | 4.66 | 9.98 | 14.98 | 20.63 | 26.61 |
| epoll | **2.66** | **5.32** | **6.98** | **8.98** | **13.30** |

## 3. Analysis

**Epoll's footprint does not grow with load.** Resident memory is flat across the
entire range: one process, one stack, one allocator arena, none of which multiply
per connection. Part of this is architectural and part is an artifact worth naming
— `users[MAX_CLIENTS]` is preallocated at startup, so the memory for 50 clients is
already committed before the first connects. A dynamically allocated variant would
still be far below the other two, but not perfectly flat.

**Threads cost more resident memory than processes.** This inverts the usual
"threads are lightweight" intuition, and the virtual-memory column explains why:
the threaded server grows by exactly **72 MB of address space per client**, which
is an 8 MB thread stack plus a 64 MB glibc malloc arena. Every thread gets its own
arena because the arena cap is 8 × cores = 160 on this machine, so all 50 qualify.
A forked child instead shares the parent's pages copy-on-write and pays only for
what it dirties — 108 KB against the threaded model's 323 KB.

**Most of that 72 MB is reservation, not memory.** VmSize measures address space;
VmRSS measures committed pages. An 8 MB stack costs 8 MB of the former and a couple
of pages of the latter. Reporting VmSize as "memory used" would overstate the
threaded server by roughly 200×. This is the single most important distinction in
the whole report.

**Epoll trades latency for efficiency.** It uses the least CPU and the least memory
but has the *highest* median and p99 latency. It is single-threaded, so all 50
clients serialise through one event loop, while fork and thread spread across 20
cores. Lowest resource consumption and lowest latency are different objectives, and
on a multi-core machine under this workload the event loop wins the first and loses
the second.

## 4. Bottlenecks

- **Threaded model, allocator pressure.** The dominant per-client cost is not the
  thread itself but its private malloc arena. `MALLOC_ARENA_MAX` would cap this
  directly; `pthread_attr_setstacksize` addresses the (much smaller) stack
  component. A thread pool would bound both.
- **Threaded model, lock contention.** A single global `clients_mutex` serialises
  every access to the user table, and the threaded server's CPU rises fastest with
  load (26.6 % at 50 clients, the highest of the three).
- **Fork model, IPC.** Broadcast requires opening, writing and closing one FIFO per
  recipient, so a broadcast to N users costs 3N syscalls.
- **Epoll model, no parallelism.** One event loop cannot use more than one core;
  this is what shows up as the highest latency despite the lowest CPU. One epoll
  instance per core with `SO_REUSEPORT` would address it.

## 5. Defects found and fixed

The first benchmark round measured the harness more than the servers. Four code
defects and three measurement defects were identified and corrected.

**Code**

1. **~40 ms floor under every round trip.** `send_message()` wrote the 3-byte
   header and the payload as two separate `send()` calls. Nagle's algorithm held
   the second write until the peer ACKed the first, and the peer's delayed-ACK
   timer only fires after ~40 ms. Coalescing into a single write took the median
   round trip from **49 ms to 0.15 ms (~350×)**. Until this was fixed all three
   architectures looked identical on latency because the stall dominated
   everything else — the original report's claim that epoll had the lowest latency
   was measuring this bug, and with it removed epoll is in fact the *slowest*.
2. **Stack buffer overflow at scale.** The user-list reply was built by `strcat`ing
   ~21-byte entries into `char user_list[512]`, which overflows once ~24 users are
   online. Every run at 30 clients and above was corrupting the stack. Verified
   fixed: at 31 online users the reply is 655 bytes and well-formed on all three
   servers.
3. **Negative round-trip times.** Timing used `gettimeofday()`, i.e.
   `CLOCK_REALTIME`, which can jump backwards; under WSL2 the VM resyncs its clock
   against the host and produced RTTs of about −717 ms. Switched to
   `CLOCK_MONOTONIC`.
4. **`listen()` backlog of 10** while 50 clients connected simultaneously, so the
   kernel silently dropped SYNs and some clients never appeared in the results.

**Measurement**

5. **CPU was unmeasurable.** `ps -o %cpu` averages over the whole process lifetime,
   so on a server that spends most of its life blocked it read 0.0 at every load
   level — the entire CPU column was zeros.
6. **State carried across load levels.** One server process served all five load
   levels, so the threaded 50-client figure was a high-water mark over ~150
   cumulative sessions rather than the cost of 50 live clients. Processes return
   everything on exit, so the fork model was not distorted the same way and the two
   were not comparable.
7. **No sustained load.** Each client timed one request and then slept, leaving the
   server idle for the entire sampling window and taking its single latency sample
   during the connect stampede rather than in steady state.

## 6. Corrections to the previous report

| Previous claim | Corrected finding |
|---|---|
| "epoll provided the most consistent and lowest latency" | epoll has the **highest** median and p99 latency. The earlier data was dominated by the ~40 ms Nagle stall, which affected all three equally. |
| "server_fork showed a significant CPU spike around 40 clients" | Not supported — the entire CPU column was zeros. Corrected measurement shows CPU rising monotonically for all three, with no fork spike. |
| "handled 100 clients with negligible overhead" | The harness never exceeded 50 clients, and `server_epoll` is compiled with `MAX_CLIENTS 50`. |
| Threaded memory attributed to "the default 8MB stack size" | The stack is a virtual reservation and contributes little resident memory. The dominant cost is the per-thread 64 MB malloc arena. |
| "fork context switching led to thrashing" at 40+ clients | No evidence. Fork uses *less* CPU than the threaded model at every load level tested. |

## 7. Recommended optimizations

1. **Multi-core epoll** — one event loop per core with `SO_REUSEPORT`, which
   addresses epoll's only weakness here without giving up its resource profile.
2. **Cap allocator arenas** in the threaded server (`MALLOC_ARENA_MAX=1..2`), the
   single largest lever on its memory, ahead of tuning stack size.
3. **Thread pool** instead of unbounded thread-per-client, to bound both memory and
   thread-creation cost.
4. **Replace FIFO fan-out** in the fork server with a shared-memory ring buffer, to
   remove the 3 syscalls per broadcast recipient.
5. **Set `TCP_NODELAY`** in addition to coalescing writes, so future multi-write
   paths cannot silently reintroduce the delayed-ACK stall.
