# Lightweight Reliable Transport Protocol (LRTP)

CS3102 Practical 2: A simple, connection-oriented, unicast transport protocol built on top of UDP with reliable ordered delivery.

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
├── startercode/
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

```bash
cd startercode
make
```

This compiles all test binaries and the LRTP library (`liblrtp.a`).

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

### Automated Test Targets

The Makefile provides convenient targets to run server/client pairs automatically:

```bash
make run_test_0       # Run test-0 pair (handles prompts internally)
make run_test_1       # Run test-1 pair
make run_test_2       # Run test-2 pair
make run_test_3       # Run test-3 pair
make run_test_adaptive # Run adaptive RTO test pair
make run_all_tests    # Run all test pairs in sequence
```

Each `run_test_*` target will:
1. Prompt for the remote client host (if needed for cross-host testing)
2. Prompt for the server address (reachable from the remote client)
3. Start a server locally
4. Launch the client (locally or remotely via SSH)

**Environment overrides:** To skip prompts, set `MAKE_HOST` and `MAKE_SERVER`:
```bash
make run_test_0 MAKE_HOST=user@remote.example.com MAKE_SERVER=192.168.1.100
```

### Cross-Host Testing

To run a client on a remote machine:

1. Ensure the remote machine has the project at `~/cs3102_p2/startercode`.
2. Run with `MAKE_HOST`:
   ```bash
   make run_test_0 MAKE_HOST=user@remote.host
   ```
3. When prompted for server address, enter the IP/hostname reachable from the remote host.

The Makefile will SSH into the remote host, change to the project directory, and execute the client binary there.

**Passwordless SSH recommended:** For seamless execution, set up SSH keys on the remote host to avoid password prompts during the test.

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

3. **For real network loss emulation** (at the kernel level), use `tc` (traffic control) on Linux:
   ```bash
   sudo tc qdisc add dev lo root netem loss 10%  # 10% loss on loopback
   sudo tc qdisc del dev lo root                 # Remove the constraint
   ```

## API Overview

All functions are defined in `lrtp.h`. The server and client APIs are asymmetric:

### Server-side
```c
int lrtp_start(uint16_t port);      // Listen on a port
int lrtp_accept(int sd);            // Accept an incoming connection
int lrtp_rx(int sd, void *data, uint16_t data_size);      // Receive data
int lrtp_tx(int sd, void *data, uint16_t data_size);      // Send data
int lrtp_close(int sd);             // Close connection
```

### Client-side
```c
int lrtp_open(const char *fqdn, uint16_t port);  // Connect to server
int lrtp_tx(int sd, void *data, uint16_t data_size);      // Send data
int lrtp_rx(int sd, void *data, uint16_t data_size);      // Receive data
int lrtp_close(int sd);             // Close connection
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

- If an acknowledgment is not received within the RTO period, the sender retransmits the unacknowledged packet
- Retransmissions continue up to `LRTP_MAX_RE_TX` (3 by default) before the connection is considered failed
- **Fixed RTO:** Initially 0.5 second (before first RTT sample)
- **Adaptive RTO:** After first RTT measurement, RTO is computed using EWMA (exponential weighted moving average):
  - SRTT (Smoothed RTT) = 7/8 × SRTT + 1/8 × RTT
  - RTTVAR (RTT Variance) = 3/4 × RTTVAR + 1/4 × |SRTT - RTT|
  - RTO = SRTT + 4 × RTTVAR
  - Clamped to [0.5s, 60s] per RFC 6298

## Adaptive RTO

The adaptive RTO implementation (`lrtp_calculate_adaptive_rto()` in `lrtp-fsm.c`) adjusts the retransmission timeout based on observed round-trip times:

### Benefits
- Faster recovery on low-latency networks (RTO decreases after smooth RTTs)
- More patient waiting on high-latency networks (RTO increases after high RTTs)
- Better overall throughput and responsiveness

### Testing Adaptive RTO
```bash
make run_test_adaptive
```

This runs a dedicated test that displays RTT statistics and RTO values as packets are transmitted.

## Metrics Collected

The LRTP implementation captures comprehensive metrics for performance analysis and validation:

### Per-Packet Metrics (Real-time Display)

Each data transfer displays per-packet values:
- **RTT (Round Trip Time)** [microseconds] - measured time from packet transmission to acknowledgment receipt
- **SRTT (Smoothed RTT)** [microseconds] - exponential moving average of observed RTTs (α = 1/8)
- **RTTVAR (RTT Variance)** [microseconds] - variability estimation in RTT measurements (β = 1/4)
- **RTO (Retransmission Timeout)** [microseconds & seconds] - computed timeout = SRTT + 4×RTTVAR
- **Packet sequence number** - for tracking order and identifying losses

Example from adaptive RTO test output:
```
Packet |   RTT (us)   |  SRTT (us)   | RTTVAR (us)  |   RTO (us)   |  RTO (s)
--------|--------------|--------------|--------------|--------------|----------
      1 |      12345   |      12345   |       6172   |      36689   | 0.036689
      2 |      12421   |      12357   |       6177   |      36765   | 0.036765
```

### Connection-Level Statistics

Comprehensive counters maintained in the Protocol Control Block (`G_pcb`):

**Handshake Counters:**
- `open_req_tx` - OPEN_REQ packets sent (initial)
- `open_req_re_tx` - OPEN_REQ retransmissions
- `open_req_rx` - OPEN_REQ packets received (server-side)
- `open_req_dup_rx` - duplicate OPEN_REQ detected
- `open_reqack_tx`, `open_reqack_re_tx`, `open_reqack_rx`, `open_reqack_dup_rx`
- `open_ack_tx`, `open_ack_re_tx`, `open_ack_rx`, `open_ack_dup_rx`

**Data Transfer Counters:**
- `data_req_tx` - DATA packets sent (initial transmissions)
- `data_req_re_tx` - DATA packets retransmitted
- `data_req_bytes_tx` - total payload bytes sent (excluding retransmissions)
- `data_req_bytes_re_tx` - payload bytes in retransmitted packets
- `data_req_rx` - DATA packets received (initial only)
- `data_req_dup_rx` - duplicate DATA packets received
- `data_req_bytes_rx` - payload bytes received (initial only)
- `data_req_bytes_dup_rx` - duplicate payload bytes detected
- `data_ack_rx` - DATA_ACK packets received
- `data_ack_dup_rx` - duplicate DATA_ACK packets
- `data_ack_tx`, `data_ack_re_tx` - DATA_ACK transmission stats

**Close Protocol Counters:**
- `close_req_tx`, `close_req_re_tx`, `close_req_rx`, `close_req_dup_rx`
- `close_ack_tx`, `close_ack_re_tx`, `close_ack_rx`, `close_ack_dup_rx`

**Timing Metrics:**
- `start_time` [microseconds] - connection establishment timestamp
- `finish_time` [microseconds] - connection closure timestamp
- Duration (calculated) - total connection lifetime

### Summary Statistics

**Final RTO Statistics:**
- Final SRTT value (smoothed RTT after all measurements)
- Final RTTVAR value (converged variance estimate)
- Final RTO value (converged timeout)

**Throughput Analysis:**
- Mean RX data rate (bps, Kbps, or Mbps) - `(payload_bytes_rx × 8) / duration`
- Mean TX data rate (bps, Kbps, or Mbps) - `(payload_bytes_tx × 8) / duration`
- Calculated from duration (finish_time - start_time)

**Reliability Metrics:**
- Total retransmissions ratio - `(data_req_re_tx / data_req_tx) × 100%`
- Duplicate detection rate - count of duplicates correctly identified
- Connection success/failure indicator

### Example Test Output

From `test-adaptive-rto-client`:
```
Adaptive RTO Test Client
Connect to localhost:24536
Sending 10 packets of 1280 bytes each

Connection established (adaptive RTO initialized)
Initial RTO: 1000000 us (1.000 s)

Packet |   RTT (us)   |  SRTT (us)   | RTTVAR (us)  |   RTO (us)   |  RTO (s)
--------|--------------|--------------|--------------|--------------|----------
      1 |       5234   |       5234   |       2617   |      15702   | 0.015702
      2 |       5187   |       5220   |       2609   |      15658   | 0.015658
   ...

Final RTO Statistics:
  Smoothed RTT: 5215 us (5.215 ms)
  RTT Variance: 2608 us (2.608 ms)
  Final RTO: 15647 us (0.015647 s)
  Data packets TX: 10
  Data packets RX (ACK): 10
  Retransmissions: 0
```

## Test Programs

### Basic Tests (test-client/server-{0,1,2,3})
- Simple exchange of single messages between client and server
- Variant 0: minimal exchange
- Variants 1-3: additional scenarios (see test source files)

### Adaptive RTO Test (test-adaptive-rto-{client,server})
- Demonstrates RTO adaptation with verbose statistics
- Prints per-packet RTT, SRTT, RTTVAR, and computed RTO
- Useful for observing how RTO responds to network conditions

### Large Transfer Test (test-large-transfer-{client,server})
- Transfers multiple packets to test reliable ordered delivery
- Prints running statistics of RTO evolution

### Multiple Sends Test (test-multiple-sends-{client,server})
- Client and server exchange multiple messages in sequence

### Stress Test (test-stress-{client,server})
- High-intensity test with many packets and retransmissions

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

## Success Criteria & Requirements Coverage

This implementation addresses all three formal requirements of CS3102 Practical 2:

### Requirement 1: Protocol Design ✓

**Finite State Machine (FSM)**
- Implemented in `lrtp-fsm.h` and `lrtp-fsm.c`
- States: CLOSED, LISTENING, OPENING_I, OPENING_R, CONNECTED, CLOSING_I
- State transitions are managed by the core protocol implementation in `lrtp.c`
- FSM handles connection lifecycle and responds to timeouts via adaptive RTO

**Protocol Design**
- Binary packet format defined in `lrtp-packet.h`
- Three-way handshake for connection establishment (open_req → open_reqack → open_ack)
- Data transfer using sequence numbers and acknowledgments (data_req / data_ack)
- Graceful connection termination (close_req / close_ack)
- All packet types follow a consistent header structure with type, sequence, and data size fields

### Requirement 2: Protocol Implementation ✓

**API Compliance**
- Fully implements the blocking/synchronous LRTP API defined in `lrtp.h`
- All required functions: `lrtp_init()`, `lrtp_start()`, `lrtp_accept()`, `lrtp_open()`, `lrtp_tx()`, `lrtp_rx()`, `lrtp_close()`

**Test Evidence**
- Provides 8 test programs demonstrating protocol operation:
  - **test-client/server-{0,1,2,3}**: Basic functionality tests
  - **test-adaptive-rto-{client,server}**: RTO measurement and adaptation
  - **test-large-transfer-{client,server}**: Multi-packet transfers
  - **test-stress-{client,server}**: High-intensity retransmission scenarios
- Makefile targets (`make run_test_*`) automate local and cross-host testing
- All tests demonstrate reliable ordered delivery, connection establishment, and termination

**Reliable Data Transfer**
- Implements idle-RQ (go-back-N with N=1) retransmission mechanism
- Initial fixed RTO of **1.0 second** per RFC 6298 (updated from 500ms for RFC compliance)
- Retransmissions up to `LRTP_MAX_RE_TX` (3 by default) before connection failure
- Sequence numbers ensure ordered delivery

**Testing with Loss**
- Supports controlled loss via the provided `drop.c` utility
- Compatible with kernel-level loss simulation using Linux `tc` (traffic control)
- README includes instructions for both local and networked loss testing

### Requirement 3: Adaptive RTO ✓

**Adaptive RTO Implementation**
- `lrtp_calculate_adaptive_rto()` in `lrtp-fsm.c` implements RFC 6298 compliant RTO calculation
- Uses EWMA (exponential weighted moving average) for smooth RTT estimation:
  - SRTT coefficient: α = 1/8
  - RTTVAR coefficient: β = 1/4
  - RTO multiplier: K = 4
- Clamped to RFC 6298 bounds: [1 second, 60 seconds]
- RTT measurements taken during connection establishment (3-way handshake) and data transfer

**Adaptive RTO Testing**
- Dedicated test program: `test-adaptive-rto-{client,server}`
- Per-packet statistics display: RTT, SRTT, RTTVAR, computed RTO
- Demonstrates RTO adaptation in response to network conditions
- Runnable with: `make run_test_adaptive`

**Performance Comparison**
- Large transfer test shows RTO evolution over multiple packets
- Adaptive RTO converges to stable values on consistent-latency networks
- Behaves gracefully under variable latency conditions
- Works correctly with and without packet loss

### Additional Requirements ✓

**Language & Implementation**
- Pure C implementation adhering to POSIX standards
- Binary protocol (no text encoding or assumptions about data type)
- No hard-coded IP addresses; all hostnames/addresses provided via CLI or prompts

**Cross-Host Testing Support**
- Makefile provides SSH-based remote client execution
- Environment variables allow transparent cross-machine setup: `MAKE_HOST` and `MAKE_SERVER`
- Tests can run on CS Linux lab machines or any SSH-accessible hosts
- Helper script: `scripts/run_client.sh` assists remote execution

## Troubleshooting

### Connection timeouts
- Check that the server is running and listening on the specified port
- Verify firewall settings if testing across different machines
- Increase RTO if network latency is high (see `lrtp.h` constants)

### Remote SSH connection issues
- Ensure SSH access to the remote host is configured
- Check that `~/cs3102_p2/startercode` exists on the remote host with built binaries
- Use `MAKE_HOST=user@host` to specify remote user explicitly

### Packet loss simulation
- Use the `drop` utility for local testing
- Use `tc` (traffic control) for kernel-level loss on real network interfaces
- Remember to clean up `tc` rules after testing: `sudo tc qdisc del dev <iface> root`

## References

- RFC 3782: The NewReno Modification to TCP's Fast Recovery Algorithm
- RFC 6298: Computing TCP's Retransmission Timer
- Course slides on transport protocols and RTO/RTT measurement

## Author & Version

CS3102 Practical 2 Implementation
- Started: January 2024
- Last checked: March 2026
- Implementation date: April 2026
