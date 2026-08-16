#!/bin/bash
# Samples one chat server's CPU, memory and live connection count.
#
# CPU is a delta measured over a short window from /proc/<pid>/stat rather than
# `ps -o %cpu`, which reports an average over the whole process lifetime -- on a
# server that spends most of its life blocked that rounds to 0.0 every time.
#
# Usage: monitor.sh <pid> <server_type> <num_clients> <rep> <out_file> [window_secs]

PID=$1
SERVER_TYPE=$2
NUM_CLIENTS=$3
REP=$4
OUT_FILE=$5
WINDOW=${6:-3}

if [ -z "$PID" ] || [ ! -d /proc/$PID ]; then
    echo "monitor: server pid '$PID' is not running" >&2
    exit 1
fi

CLK_TCK=$(getconf CLK_TCK)

# The fork server does all its work in children, so they have to be counted too.
pid_set() {
    echo $PID
    if [ "$SERVER_TYPE" == "fork" ]; then
        pgrep -P $PID
    fi
}

# utime+stime in clock ticks. comm sits in parens and may itself contain spaces
# or parens, so drop everything through the last ") " before indexing fields:
# after that, stat field 14 (utime) is $12 and field 15 (stime) is $13.
pid_ticks() {
    local rest
    rest=$(sed 's/.*) //' /proc/$1/stat 2>/dev/null) || return 1
    [ -z "$rest" ] && return 1
    echo "$rest" | awk '{print $12 + $13}'
}

declare -A TICKS_START
for p in $(pid_set); do
    t=$(pid_ticks $p) && TICKS_START[$p]=$t
done
WALL_START=$(date +%s.%N)

sleep $WINDOW

DELTA=0
for p in "${!TICKS_START[@]}"; do
    t=$(pid_ticks $p) || continue   # exited mid-window; its last ticks are lost
    DELTA=$((DELTA + t - ${TICKS_START[$p]}))
done
WALL_END=$(date +%s.%N)

CPU=$(awk -v d="$DELTA" -v hz="$CLK_TCK" -v s="$WALL_START" -v e="$WALL_END" \
      'BEGIN { printf "%.2f", (e > s) ? (d / hz) / (e - s) * 100 : 0 }')

VMRSS=0; VMSIZE=0; PSS=0; SOCKETS=0
for p in $(pid_set); do
    rss=$(awk   '/^VmRSS:/  {print $2}' /proc/$p/status 2>/dev/null)
    size=$(awk  '/^VmSize:/ {print $2}' /proc/$p/status 2>/dev/null)
    pss=$(awk   '/^Pss:/ {sum += $2} END {print sum + 0}' /proc/$p/smaps 2>/dev/null)
    VMRSS=$((VMRSS   + ${rss:-0}))
    VMSIZE=$((VMSIZE + ${size:-0}))
    PSS=$((PSS       + ${pss:-0}))
    SOCKETS=$((SOCKETS + $(ls -l /proc/$p/fd 2>/dev/null | grep -c 'socket:')))
done

# Each model holds exactly one listening socket; everything else is a client.
# Logging this makes the run auditable -- if conns < num_clients, some clients
# never got connected and that row's memory figure is measuring the wrong thing.
CONNS=$((SOCKETS - 1))

echo "$SERVER_TYPE,$NUM_CLIENTS,$REP,$CPU,$VMRSS,$VMSIZE,$PSS,$CONNS" >> $OUT_FILE
