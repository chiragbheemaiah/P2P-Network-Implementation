#!/usr/bin/env bash
# Usage: ./testing.sh <N>
# Spawns N containers on user-defined network 192.168.100.0/24 starting at .2

set -euo pipefail

# Config
SUBNET_CIDR="192.168.100.0/24"
IP_PREFIX="192.168.100."
INITIAL_IP=2            # first host IP to use (192.168.100.2)
NETWORK_NAME="Gaul"
IMAGE="gcc:14"          

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <N_CONTAINERS>"
  exit 1
fi
N="$1"

echo -e "\n Creating the Gaul World"
# Ensure network exists (idempotent)
if ! docker network inspect "$NETWORK_NAME" >/dev/null 2>&1; then
  docker network create --subnet "$SUBNET_CIDR" "$NETWORK_NAME"
fi

# Last IP we will assign
LAST_IP=$(( INITIAL_IP + N - 1 ))
if (( LAST_IP > 254 )); then
  echo -e " \n Error: Requested $N containers exceeds subnet host range."
  exit 1
fi

# Launch containers
for (( i=INITIAL_IP, idx=1; i<=LAST_IP; i++, idx++ )); do
  IP_ADDRESS="${IP_PREFIX}${i}"
  CONTAINER_NAME="Container-${idx}"

  # Per-node config/args
  SERVER_IP="${IP_ADDRESS}"
  ID="${idx}"

  # Ring topology for NEIGHBOR_SERVER_IP
  if (( i < LAST_IP )); then
    NEIGHBOR_SERVER_IP="${IP_PREFIX}$(( i + 1 ))"
    SECRET="Thor"
  else
    NEIGHBOR_SERVER_IP="${IP_PREFIX}${INITIAL_IP}"
    SECRET="Apple"
  fi

  # Mark first container as source
  if (( i == INITIAL_IP || idx == 2)); then
    IS_SOURCE="true"
    QUERY="Apple"
  else
    IS_SOURCE="false"
    QUERY="None"
  fi

  echo -e " \nLaunching ${CONTAINER_NAME} at ${IP_ADDRESS} (neighbor ${NEIGHBOR_SERVER_IP})"

  # Mount current directory so the container can see node.cpp and compile.sh
  # Then run compile.sh, which compiles and execs NODE in foreground.
  docker run -d --name "$CONTAINER_NAME" --rm \
    --network "$NETWORK_NAME" --ip "$IP_ADDRESS" \
    -v "$PWD":/app -w /app \
    "$IMAGE" bash -c "chmod +x ./compile.sh && ./compile.sh '${ID}' '${SERVER_IP}' '${NEIGHBOR_SERVER_IP}' '${IS_SOURCE}' '${SECRET}' '${QUERY}' '${N}'"
done
