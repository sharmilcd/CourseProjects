mkdir -p ../logs

SERVER=${1:-server_epoll}
SERVER_TYPE=$(echo $SERVER | cut -d'_' -f2)

echo "Starting Load Test: 10 clients on $SERVER..."

../discovery_server > /dev/null 2>&1 &
DISC_PID=$!
sleep 1

../$SERVER > /dev/null 2>&1 &
CHAT_PID=$!
sleep 1

for i in {1..10}; do
    ../benchmark_client "loaduser${i}" "pass" "$SERVER_TYPE" > /dev/null 2>&1 &
done

echo "Running... waiting 10 seconds for message delivery."
sleep 10

pkill -f benchmark_client
kill -9 $CHAT_PID $DISC_PID
echo "Load test complete."
