# P2P Ring Network — Summary

A Dockerized peer-to-peer system with **N** nodes arranged in a **ring**. Any node can issue a `QUERY(item)` that forwards clockwise until a seller is found; matching nodes send a **confirmation** back, and the buyer then opens a **direct TCP connection** (`BUY`) to complete the transaction. A lightweight Python launcher spins up the containers, creates the network, and wires neighbor links.

Change the directory to P2P and run the following:

**Requirements:** Docker, Python 3
**Run:**

```bash
python3 launch_script.py <NUMBER_OF_NODES>
```

