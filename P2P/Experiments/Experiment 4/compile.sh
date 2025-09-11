#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 8 ]]; then
  echo -e "\nUsage: <ID> <SERVER_IP> <NEIGHBOR_SERVER_IP> <IS_SOURCE> <SECRET> <QUERY> <HOP_COUNT> <EXP_FILE_NAME>"
  exit 1
fi

ID="$1"
SERVER_IP="$2"
NEIGHBOR_SERVER_IP="$3"
IS_SOURCE="$4"
SECRET="$5"
QUERY="$6"
HOP_COUNT="$7"
EXP_FILE_NAME="$8"


echo -e "\n================ Parameters ================\n"
echo "ID                : $ID"
echo "SERVER_IP         : $SERVER_IP"
echo "NEIGHBOR_SERVER_IP: $NEIGHBOR_SERVER_IP"
echo "IS_SOURCE         : $IS_SOURCE"
echo "SECRET            : $SECRET"
echo "QUERY             : $QUERY"
echo "HOP_COUNT         : $HOP_COUNT"
echo "EXP_FILE_NAME     : $EXP_FILE_NAME"
echo -e "\n===========================================\n"

echo -e "\nCompilation Phase .......\n"
echo -e "=========================================="
echo -e "\nCompiling Node ......\n"

g++ node.cpp -o NODE -pthread

echo -e "\nLaunching node.......\n"
echo -e "=========================================="

NUM_THREADS=4
SERVER_PORT_NUMBER=8080
CONNECTION_NUMBER=5
NEIGHBOR_PORT_NUMBER=8080

echo -e "\n Launching Node ......\n"

exec ./NODE "$ID" "$NUM_THREADS" "$SERVER_IP" "$SERVER_PORT_NUMBER" \
            "$CONNECTION_NUMBER" "$NEIGHBOR_SERVER_IP" "$NEIGHBOR_PORT_NUMBER" \
            "$IS_SOURCE" "$SECRET" "$QUERY" "$HOP_COUNT" "$EXP_FILE_NAME"
