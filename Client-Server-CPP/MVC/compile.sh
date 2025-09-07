#!/bin/bash

set -e

echo -e "\n Compilation Phase .......\n"
echo -e "=========================================="
echo -e "\n Compiling Node ......\n"
g++ mega_node.cpp -o NODE -pthread

echo -e "\nLaunching nodes.......\n"
echo -e "=========================================="

# Arguments: <ID> <NUM_THREADS> <SERVER_IP> <SERVER_PORT_NUMBER> <CONNECTION_NUMBER> \
#            <NEIGHBOR_SERVER_IP> <NEIGHBOR_PORT_NUMBER> <IS_SOURCE:BOOL> <SECRET> <QUERY_STRING> <HOP_COUNT>

echo -e "\n Launching Node A......\n"
./NODE A 4 127.0.0.1 8090 5 127.0.0.1 8091 true Thor Apple 5 &

echo -e "\n Launching Node B......\n"
./NODE B 4 127.0.0.1 8091 5 127.0.0.1 8092 false Mjnolir None 5 &

echo -e "\n Launching Node C......\n"
./NODE C 4 127.0.0.1 8092 5 127.0.0.1 8090 false Apple None 5 &
