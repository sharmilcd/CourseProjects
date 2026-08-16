#!/bin/bash

echo "Nuking all chat system processes..."

pkill -9 -f discovery_server
pkill -9 -f server_fork
pkill -9 -f server_thread
pkill -9 -f server_epoll
pkill -9 -f chat_client
pkill -9 -f benchmark_client

echo "Cleanup complete! All ports (8080, 9000) should now be free."