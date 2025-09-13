# P2P Ring Network — Summary

A Dockerized peer-to-peer system with **N** nodes arranged in a **ring**.

Any node can issue a `QUERY(item)` that forwards clockwise until a seller is found.
Matching nodes send a **confirmation** back, and the buyer then opens a **direct TCP connection** (`BUY`) to complete the transaction.

A lightweight Python launcher spins up the containers, creates the network, and wires neighbor links.

---

### How to Run

**Requirements:** Docker, Python 3

Change into the `P2P` directory and run the following:

```bash
python3 launch_script.py <NUMBER_OF_NODES>
```

---

### Learn More

Curious about the design decisions, challenges, and performance analysis behind this system? The blog post walks through the full journey — from initial concept to detailed Docker stats breakdowns and lessons learned in building a peer-to-peer distributed system.

👉 **Read the Dev Diary:** [Building the P2P Ring Network](https://chiragbheemaiah.github.io/P2P-Network-Implementation/)

Take a look if you want to dive deeper into distributed networking, Docker orchestration, and what it takes to get a lightweight peer-to-peer architecture running in practice.
