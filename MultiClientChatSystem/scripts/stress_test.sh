#!/bin/bash
# Concurrent-connection scaling test across the three server architectures.
#
# Every data point gets a freshly started server. The previous version reused a
# single server process for all five load levels, which meant the 50-client
# sample also carried whatever the 10/20/30/40-client rounds had left behind --
# glibc caches exited threads' stacks and never returns secondary arenas, so the
# threaded numbers were a high-water mark over ~150 cumulative sessions rather
# than the cost of 50 live clients. Processes return everything on exit, so the
# fork model was not distorted the same way and the two were not comparable.

set -u

mkdir -p ../logs ../data
rm -f ../logs/metrics.csv ../logs/delivery_times.csv
echo "server_type,num_clients,rep,cpu,vmrss,vmsize,pss,conns" > ../logs/metrics.csv

MAX_CLIENTS=50
STEP=10
SETTLE=5        # seconds for clients to connect and reach steady state
WINDOW=3        # CPU sampling window
REPS=3          # repeats per data point; the threaded server's allocator makes
                # any single sample noisy, so report the median of several

DISC_PID=""
CHAT_PID=""

teardown() {
    pkill -f benchmark_client >/dev/null 2>&1
    [ -n "$CHAT_PID" ] && kill -9 "$CHAT_PID" >/dev/null 2>&1
    [ -n "$DISC_PID" ] && kill -9 "$DISC_PID" >/dev/null 2>&1
    # kill -9 on the fork parent orphans its children; they must go explicitly
    pkill -9 -x server_fork   >/dev/null 2>&1
    pkill -9 -x server_thread >/dev/null 2>&1
    pkill -9 -x server_epoll  >/dev/null 2>&1
    pkill -9 -x discovery_server >/dev/null 2>&1
    rm -f /tmp/chat_user* >/dev/null 2>&1
    wait 2>/dev/null
}
trap teardown EXIT

for SERVER in server_fork server_thread server_epoll; do
    SERVER_TYPE=$(echo $SERVER | cut -d'_' -f2)
    echo "=== Testing $SERVER ==="

    for (( c=10; c<=MAX_CLIENTS; c+=STEP )); do
        for (( rep=1; rep<=REPS; rep++ )); do
            rm -rf ../data && mkdir -p ../data
            rm -f /tmp/chat_user*

            ../discovery_server > /dev/null 2>&1 &
            DISC_PID=$!
            ../$SERVER > /dev/null 2>&1 &
            CHAT_PID=$!
            sleep 1

            for (( i=1; i<=c; i++ )); do
                ../benchmark_client "user${i}" "pass${i}" "$SERVER_TYPE" > /dev/null 2>&1 &
            done

            sleep $SETTLE
            ./monitor.sh $CHAT_PID $SERVER_TYPE $c $rep ../logs/metrics.csv $WINDOW
            echo "  $c clients (rep $rep) -> $(tail -1 ../logs/metrics.csv)"

            teardown
            DISC_PID=""; CHAT_PID=""
            sleep 1
        done
    done
done

echo "Stress test complete. Data saved to ../logs/."
