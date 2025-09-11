#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys
from pathlib import Path
import random

SUBNET_CIDR = "192.168.100.0/24"
IP_PREFIX   = "192.168.100."
INITIAL_IP  = 2                 # first host IP to use (192.168.100.2)
NETWORK_NAME = "Gaul"
IMAGE        = "gcc:14"


def main():
    parser = argparse.ArgumentParser(description="Welcome to Gaul!")
    parser.add_argument("number_of_nodes", type=int, help="Number of nodes to spawn minimum 2")
    parser.add_argument("number_of_hop_count", type=int, help="Number of hop counts")
    args = parser.parse_args()

    N = args.number_of_nodes
    hop_count = args.number_of_hop_count
    exp_file_name = "metrics_hc" + str(hop_count) + ".txt"

    if N < 2:
        print("Error: number_of_nodes must be positive and greater than 1", file=sys.stderr)
        sys.exit(2)

    print("\nCreating the Gaul World")

    result = subprocess.run(
        ["docker", "network", "inspect", NETWORK_NAME],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    )
    if result.returncode != 0:
        subprocess.run(
            ["docker", "network", "create", "--subnet", SUBNET_CIDR, NETWORK_NAME],
            check=True
        )

    last_ip = INITIAL_IP + N - 1
    if last_ip > 254:
        print(f"Error: Requested {N} nodes exceeds subnet host range.", file=sys.stderr)
        sys.exit(1)

    # ---- Market Setup -----
    items = ["fish", "boar", "salt"]

    number_of_buyers = 3 # arbitrary choice
    random_buyer_ids = []
    while len(random_buyer_ids) != number_of_buyers:
        random_buyer_id = random.randint(1, N)
        if random_buyer_id not in random_buyer_ids:
            random_buyer_ids.append(random_buyer_id)

    cwd = str(Path.cwd())

    # Launch containers
    for idx, ip in enumerate(range(INITIAL_IP, last_ip + 1), start=1):
        ip_addr = f"{IP_PREFIX}{ip}"
        name = f"Container-{idx}"

        # Ring topology + secrets
        if ip < last_ip:
            neighbor_ip = f"{IP_PREFIX}{ip + 1}"
        else:
            neighbor_ip = f"{IP_PREFIX}{INITIAL_IP}"

        # what this container is selling
        secret = random.choice(items)

        # buyer or not
        if idx in random_buyer_ids:
            is_source = "true"
            query = random.choice(items)
            role_info = f"BUYER (buying {query})"
        else:
            is_source = "false"
            query = "None"
            role_info = "NON-BUYER"

        print(f"\nLaunching {name} at {ip_addr} (neighbor {neighbor_ip}) "
              f"[sells {secret}] [{role_info}]")

        # Start container; mount PWD to /app and run compile.sh with args
        subprocess.run([
            "docker", "run", "-d",
            "--name", name,
            "--network", NETWORK_NAME, "--ip", ip_addr,
            "-v", f"{cwd}:/app", "-w", "/app",
            IMAGE, "bash", "-lc",
            f"chmod +x ./compile.sh && ./compile.sh '{idx}' '{ip_addr}' "
            f"'{neighbor_ip}' '{is_source}' '{secret}' '{query}' '{hop_count}' '{exp_file_name}'"
        ], check=True)


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as e:
        print(f"\nCommand failed: {e}", file=sys.stderr)
        sys.exit(e.returncode)
