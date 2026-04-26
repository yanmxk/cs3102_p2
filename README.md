# Lightweight Reliable Transport Protocol (LRTP)

CS3102 Practical 2: A simple, connection-oriented, unicast transport protocol built on top of UDP providing reliable, ordered delivery.

## Overview

LRTP implements a lightweight, blocking/synchronous transport protocol that provides:
- **Connection-oriented communication** between a single client and server
- **Reliable ordered delivery** using the idle-RQ (go-back-N with N=1) retransmission mechanism
- **Adaptive RTO** based on RTT measurements (per RFC 6298)
- **Binary protocol** that allows transfer of arbitrary binary data

## Project Structure

```
cs3102_p2/
├── README.md                        # This file
├── drop.c                           # Packet drop simulator utility
├── code/
│   ├── Makefile                     # Build and test automation
│   ├── lrtp.h                       # LRTP API (public interface)
│   ├── lrtp.c                       # Core protocol implementation
│   ├── lrtp-fsm.{c,h}              # Finite state machine and RTO calculation
│   ├── lrtp-pcb.{c,h}              # Protocol control block definitions
│   ├── lrtp-packet.h               # Packet header definitions
│   ├── lrtp-common.{c,h}           # Common utilities and constants
│   ├── byteorder64.{c,h}           # Byte order handling
│   ├── d_print.{c,h}               # Debugging print utilities
│   └── tests/
│       ├── test-client-{0,1,2,3}.c # Basic test clients (accept port arg)
│       ├── test-server-{0,1,2,3}.c # Corresponding test servers
│       ├── test-adaptive-rto-{client,server}.c   # Adaptive RTO demo
│       ├── test-large-transfer-{client,server}.c # Large transfer test
│       ├── test-multiple-sends-{client,server}.c # Multiple message test
│       └── test-stress-{client,server}.c         # Stress test
```

## Building

Build the LRTP library and test binaries from the `startercode` directory.

```bash
cd startercode
make
```

This produces the test executables and `liblrtp.a` used by the tests.

### Clean Build

```bash
make clean
make
```

## Running Tests

### Quick Local Tests

Start a server in one terminal:
```bash
cd startercode
./test-server-0 9999
```

In another terminal, run a client:
```bash
cd startercode
./test-client-0 localhost 9999
```

## Testing with Controlled Loss

Use the provided `drop.c` utility to simulate packet loss:

1. **Build the drop utility:**
   ```bash
   clang -o drop ../drop.c
   ```

2. **Simulate loss on the server (example: 10% drop rate over 1000 packets):**
   ```bash
   ./drop 10 1000
   ```

   This will display each packet and indicate which ones are "dropped".

## API Overview

All functions are defined in `lrtp.h`. The server and client APIs are asymmetric:

### Server-side
```c
int lrtp_start(uint16_t port);                       // Listen on a port
int lrtp_accept(int sd);                             // Accept an incoming connection
int lrtp_rx(int sd, void *data, uint16_t data_size); // Receive data
int lrtp_tx(int sd, void *data, uint16_t data_size); // Send data
int lrtp_close(int sd);                              // Close connection
```

### Client-side
```c
int lrtp_open(const char *fqdn, uint16_t port);       // Connect to server
int lrtp_tx(int sd, void *data, uint16_t data_size);  // Send data
int lrtp_rx(int sd, void *data, uint16_t data_size);  // Receive data
int lrtp_close(int sd);                               // Close connection
```

### Initialization
```c
void lrtp_init();  // Initialize protocol state
```

## Protocol Design

### Connection Establishment (3-way handshake)

1. **open_req:** Client sends connection request to server
2. **open_reqack:** Server acknowledges and signals readiness
3. **open_ack:** Client confirms; connection established

### Data Transfer

- **data_req:** Sender transmits data packet with sequence number
- **data_ack:** Receiver acknowledges; data is now considered delivered

### Connection Termination

1. **close_req:** Initiator requests connection close
2. **close_ack:** Responder acknowledges; connection closed

### Retransmission (Idle-RQ with N=1)
- **Fixed RTO:** Initially 0.5 second (before first RTT sample)
- **Adaptive RTO:** After first RTT measurement, RTO is computed using EWMA (exponential weighted moving average):
  - SRTT (Smoothed RTT) = 7/8 × SRTT + 1/8 × RTT
  - RTTVAR (RTT Variance) = 3/4 × RTTVAR + 1/4 × |SRTT - RTT|
  - RTO = SRTT + 4 × RTTVAR
  - Clamped to [0.5s, 60s] per RFC 6298

### Summary Statistics

**Final RTO Statistics:**
- Final SRTT value (smoothed RTT after all measurements)
- Final RTTVAR value (converged variance estimate)
- Final RTO value (converged timeout)

**Throughput Analysis:**
- Mean RX data rate (bps, Kbps, or Mbps)
- Mean TX data rate (bps, Kbps, or Mbps)
- Calculated from duration (finish_time - start_time)

**Reliability Metrics:**
- Total retransmissions ratio
- Duplicate detection rate - count of duplicates correctly identified
- Connection success/failure indicator

## Implementation Notes

### Finite State Machine

States in each connection lifecycle:
- **CLOSED:** No connection
- **LISTENING** (server): Waiting for incoming open_req
- **OPENING_I** (client): Waiting for open_reqack
- **OPENING_R** (server): Waiting for open_ack after sending open_reqack
- **CONNECTED:** Data transfer phase
- **CLOSING_I:** Initiator waiting for close_ack
- **CLOSING_R:** Responder closing

### Protocol Control Block (PCB)

Stored in global `G_pcb` (single connection per process):
- Socket descriptor and addressing
- Sequence numbers (tx/rx)
- RTO and adaptive RTO fields (srtt, rttvar, rto)
- Counters for debugging and analysis (packet counts, bytes transferred, etc.)

### Binary Protocol

All packets use a fixed header (defined in `lrtp-packet.h`):
```c
struct {
  uint8_t type;           // Packet type (open_req, data, etc.)
  uint32_t seq;           // Sequence number
  uint16_t data_size;     // Size of payload (for data_req)
  uint8_t payload[...];   // Variable-length payload
} Lrtp_Packet_t;
```

## Test Guide

This section explains how to run each test, capture logs, simulate loss conditions, and generate analysis plots.

- **Workspace scripts**: two convenience scripts are provided at the repository root:
  - `run_tests.sh` — runs a selection of local test pairs.
  - `run_drop_tests.sh` — runs tests through the `drop` utility to simulate packet loss.

- **Log locations**:
  - Standard test logs: `exemplar_logs/` (contains `test_summary.txt` and individual `.log` files)
  - Drop-run logs: `exemplar_drop_logs/` (contains `drop_summary.txt` and per-test logs)

- **Run tests via repository scripts (recommended)**    
**Note:** The folder with submission must in in the same location on both machines with identical content.

  - Run a quick set of tests and collect logs:

    ```bash
    ./run_tests.sh        # executes a pre-defined selection and saves logs to exemplar_logs/
    ```

  - Run loss-injection tests using the drop simulator:

    ```bash
    ./run_drop_tests.sh   # uses drop to simulate loss and saves logs to exemplar_drop_logs/
    ```

- **Interpreting logs and generating plots**

  - The repository includes a Jupyter notebook: `LRTP_Protocol_Analysis.ipynb` that parses logs and generates analysis plots in `plots/`.
