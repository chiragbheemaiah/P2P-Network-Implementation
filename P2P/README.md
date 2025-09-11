# P2P Ring Network

A peer-to-peer (P2P) network with **N** nodes arranged in a **ring topology**. Any node can initiate a **query** for an item; the query is forwarded to its neighbor, which checks its local inventory. If a node has the requested item, it sends a **confirmation** back to the buyer. The buyer then **directly connects to the seller** to initiate the **transaction**.

---

## Table of Contents

* [Overview](#overview)
* [Architecture](#architecture)
* [Prerequisites](#prerequisites)
* [Quick Start](#quick-start)
* [Run the Setup](#run-the-setup)
* [How It Works](#how-it-works)
* [Project Layout](#project-layout)


---

## Overview

* **Topology:** Ring
* **Query propagation:** Neighbor → neighbor
* **Match/Confirm:** Any node with the item replies with a confirmation
* **Transaction:** Buyer opens a direct connection to the seller

This keeps lookups simple and allows direct, efficient data transfer once a match is found.

## Architecture

```
       ┌───────────┐        ┌───────────┐
  ┌───▶│  Node 1   ├───▶───▶│  Node 2   │───┐
  │    └───────────┘        └───────────┘   │
  │                                          ▼
┌─┴──────────┐                          ┌───────────┐
│  Node N    │◀─────────────────────────┤  Node 3   │
└────────────┘                          └───────────┘
```

## Prerequisites

* **Docker**
* **Python 3** (used to launch and wire up the containers)

## Quick Start

From the repo root:

```bash
python3 launch_script.py <NUMBER_OF_NODES>
# example:
python3 launch_script.py 10
```

This launches N Docker containers, attaches them to a user-defined network, and links them into a ring.

## Run the Setup

```bash
python3 launch_script.py <NUMBER_OF_NODES>
```

* `NUMBER_OF_NODES` must be **≥ 2**.
* The launcher will:

  1. Create (or reuse) a user-defined Docker network.
  2. Start N instances of the node container.
  3. Assign addresses and set each node’s **neighbor** to form the ring.

## How It Works

1. A **buyer node** emits a `QUERY(item_id)` to its clockwise neighbor.
2. Each node checks its local inventory:

   * If **has item**, it sends a **confirmation** (e.g., `QUERY_RESPONSE`) back to the buyer.
   * Otherwise, it forwards the query to its neighbor (respecting a hop limit to avoid loops).
3. The buyer chooses a seller from confirmations and opens a **direct connection** to send a `BUY(item_id)` and complete the transaction.
