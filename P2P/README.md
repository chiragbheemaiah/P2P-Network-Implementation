# P2P Ring Network

A peer-to-peer (P2P) network with **N nodes** arranged in a **ring topology**. Any node may initiate a query; the query is forwarded to its neighbor, which checks whether it has the requested item. If a neighbor (or any subsequent node) has the item, it sends a **confirmation** back. The buyer that receives confirmations then **directly connects to the seller** to initiate the **transaction**.

> This repo provides a Dockerized setup and a simple Python launcher to spin up N containers and wire them into a ring.

---

## Table of Contents
- [Overview](#overview)
- [Architecture](#architecture)
- [Quick Start](#quick-start)
- [Prerequisites](#prerequisites)
- [Run the Setup](#run-the-setup)
- [How It Works](#how-it-works)
- [Project Layout](#project-layout)
- [Configuration](#configuration)
- [Troubleshooting](#troubleshooting)
- [License](#license)

---

## Overview
- **Topology:** Ring  
- **Discovery:** Neighbor-to-neighbor query passing  
- **Match:** Any node with the requested item sends a confirmation  
- **Transaction:** Buyer establishes a **direct connection** to the seller for the final exchange

This pattern keeps lookup simple while allowing direct high-bandwidth transfers once a match is found.

## Architecture

