/*
  CS3102 Coursework P2 : Lightweight Reliable Transport Protocol (LRTP).
  saleem, Jan2024, Feb2023.
  checked March 2025 (sjm55)
  revised March 2026 (sjm55)

  API for LRTP.
*/

#include <stdio.h> /* this should have 'void perror(const char *s);' */
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include "lrtp.h"
#include "lrtp-packet.h"
#include "lrtp-common.h"
#include "lrtp-fsm.h"
#include "lrtp-pcb.h"

#include "byteorder64.h"
#include "d_print.h"

extern Lrtp_Pcb_t G_pcb;

/*
  Set socket receive timeout in microseconds
  Calculates timeval struct and calls setsockopt() to set SO_RCVTIMEO on the socket.
  Returns 0 on success, -1 on failure (with errno set by setsockopt).
*/
static int set_recv_timeout(int sd, uint32_t timeout_us)
{
  struct timeval tv;
  tv.tv_sec = timeout_us / 1000000;
  tv.tv_usec = timeout_us % 1000000;
  return setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

/*
  Initialize the LRTP protocol control block and any other necessary state.
*/
void lrtp_init()
{
  reset_LrtpPcb(); // reset G_pcb to starting values
}

/*
  Set up a socket and listen for incoming connection requests on the specified port.

  Arguments:
    port: local port number to be used for socket

  Return:
    error   - lrtp-common.h
    success - valid socket descriptor
*/
int lrtp_start(uint16_t port)
{
  int sd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sd < 0)
  {
    perror("socket");
    return LRTP_ERROR;
  }

  int reuse = 1; // allow reuse of local address for quick restart after close
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
  {
    perror("setsockopt");
    close(sd);
    return LRTP_ERROR;
  }

  struct sockaddr_in local;
  memset(&local, 0, sizeof(local));
  local.sin_family = AF_INET;
  local.sin_addr.s_addr = htonl(INADDR_ANY);
  local.sin_port = htons(port);

  if (bind(sd, (struct sockaddr *)&local, sizeof(local)) < 0)
  {
    perror("bind");
    close(sd);
    return LRTP_ERROR;
  }

  // Update protocol control block with local socket info and state
  G_pcb.sd = sd;
  G_pcb.port = port;
  G_pcb.local = local;
  G_pcb.state = LRTP_state_listening;

  return sd;
}

/*
  Accept an incoming connection request on a listening socket.

  Arguments:
    sd: socket descriptor as previously provided by lrtp_start()

  Return :
    error   - lrtp-common.h
    success - sd, to indicate sd now also is connected
*/
int lrtp_accept(int sd)
{
  if (G_pcb.state != LRTP_state_listening) // must be in listening state to accept
  {
    return LRTP_ERROR_fsm;
  }

  Lrtp_Packet_t pkt;
  struct sockaddr_in remote;
  socklen_t remote_len = sizeof(remote);

  // Set timeout for initial open_req
  set_recv_timeout(sd, 5000000); // 5 sec

  // Wait for open_req with retransmissions
  memset(&pkt, 0, sizeof(pkt));
  memset(&remote, 0, sizeof(remote));

  int n = recvfrom(sd, (void *)&pkt, sizeof(pkt), 0,
                   (struct sockaddr *)&remote, &remote_len);

  if (n < 0)
  {
    perror("recvfrom open_req");
    return LRTP_ERROR;
  }
  // 1st step in 3-way handshake: receive open_req
  if (pkt.hdr.type != LRTP_TYPE_open_req)
  {
    return LRTP_ERROR;
  }

  // Update protocol control block with remote socket info, initial sequence numbers, and state
  G_pcb.remote = remote;
  G_pcb.seq_rx = pkt.hdr.seq + 1;
  G_pcb.start_time = lrtp_timestamp();
  G_pcb.open_req_rx++;

  // 2nd step in 3-way handshake: send open_reqack
  G_pcb.state = LRTP_state_opening_r; // receiver
  memset(&pkt, 0, sizeof(pkt));
  pkt.hdr.type = LRTP_TYPE_open_reqack;
  pkt.hdr.seq = 1;
  pkt.hdr.data_size = 0;

  if (sendto(sd, (void *)&pkt, sizeof(pkt.hdr), 0,
             (struct sockaddr *)&G_pcb.remote, sizeof(G_pcb.remote)) < 0)
  {
    perror("sendto");
    return LRTP_ERROR;
  }
  // Update protocol control block with open_reqack transmission count
  G_pcb.open_reqack_tx++;

  // Wait for open_ack with retransmissions
  set_recv_timeout(sd, G_pcb.rto);
  int retries = 0;

  while (retries < LRTP_MAX_RE_TX)
  {
    memset(&pkt, 0, sizeof(pkt));
    remote_len = sizeof(G_pcb.remote);

    // Record timestamp for first open_reqack sent (for RTT measurement)
    if (retries == 0 && G_pcb.tx_timestamp == 0)
    {
      G_pcb.tx_timestamp = lrtp_timestamp();
    }

    n = recvfrom(sd, (void *)&pkt, sizeof(pkt), 0,
                 (struct sockaddr *)&G_pcb.remote, &remote_len);

    if (n < 0)
    {
      // Timeout - resend open_reqack
      Lrtp_Packet_t resp;
      memset(&resp, 0, sizeof(resp));
      resp.hdr.type = LRTP_TYPE_open_reqack;
      resp.hdr.seq = 1;

      if (sendto(sd, (void *)&resp, sizeof(resp.hdr), 0,
                 (struct sockaddr *)&G_pcb.remote, sizeof(G_pcb.remote)) < 0)
      {
        perror("sendto retx");
        return LRTP_ERROR;
      }
      G_pcb.open_reqack_re_tx++;
      retries++;
      continue;
    }

    // 3rd step in 3-way handshake: receive open_ack
    if (pkt.hdr.type == LRTP_TYPE_open_ack)
    {
      /* Count duplicates if already seen an open_ack */
      if (G_pcb.open_ack_rx == 0)
      {
        G_pcb.open_ack_rx++;
        G_pcb.seq_tx++; // increment seq for next transmission
        G_pcb.state = LRTP_state_connected;

        /* Calculate RTT and update adaptive RTO on first successful reception */
        if (G_pcb.tx_timestamp > 0)
        {
          uint64_t now = lrtp_timestamp();
          uint32_t rtt = (uint32_t)(now - G_pcb.tx_timestamp);
          G_pcb.rtt = rtt;
          G_pcb.rto = lrtp_calculate_adaptive_rto(rtt, &G_pcb.srtt, &G_pcb.rttvar); // req3
        }

        return sd;
      }
      else
      {
        G_pcb.open_ack_dup_rx++;
        /* ignore duplicate and continue waiting */
      }
    }
    // If received packet is not open_ack, check if it's a duplicate open_req
    else if (pkt.hdr.type == LRTP_TYPE_open_req)
    {
      G_pcb.open_req_dup_rx++;

      // Resend open_reqack
      Lrtp_Packet_t resp;
      memset(&resp, 0, sizeof(resp));
      resp.hdr.type = LRTP_TYPE_open_reqack;
      resp.hdr.seq = 1;

      if (sendto(sd, (void *)&resp, sizeof(resp.hdr), 0,
                 (struct sockaddr *)&G_pcb.remote, sizeof(G_pcb.remote)) < 0)
      {
        perror("sendto dup");
        return LRTP_ERROR;
      }
      // Update protocol control block with duplicate open_req reception and open_reqack retransmission counts
      G_pcb.open_reqack_re_tx++;
    }
  }

  return LRTP_ERROR;
}

/*
  Open a connection to a server at the specified fully qualified domain name (FQDN) and port.

  Arguments:
    fqdn: fully qualified domain name of the server
    port: local and remote port number to be used for socket

  Return:
    error - LRTP_ERROR
    success - valid socket descriptor
*/
int lrtp_open(const char *fqdn, uint16_t port)
{
  int sd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sd < 0)
  {
    perror("socket");
    return LRTP_ERROR;
  }

  struct hostent *he = gethostbyname(fqdn);
  if (he == NULL)
  {
    herror("gethostbyname");
    close(sd);
    return LRTP_ERROR;
  }

  // Set up remote and local socket addresses
  struct sockaddr_in remote;
  memset(&remote, 0, sizeof(remote));
  remote.sin_family = AF_INET;
  remote.sin_port = htons(port);
  memcpy(&remote.sin_addr, he->h_addr_list[0], he->h_length);

  struct sockaddr_in local;
  memset(&local, 0, sizeof(local));
  local.sin_family = AF_INET;
  local.sin_addr.s_addr = htonl(INADDR_ANY);
  local.sin_port = htons(0);

  if (bind(sd, (struct sockaddr *)&local, sizeof(local)) < 0)
  {
    perror("bind");
    close(sd);
    return LRTP_ERROR;
  }

  socklen_t local_len = sizeof(local);
  if (getsockname(sd, (struct sockaddr *)&local, &local_len) < 0)
  {
    perror("getsockname");
    close(sd);
    return LRTP_ERROR;
  }

  // Update protocol control block with socket info and initial state
  G_pcb.sd = sd;
  G_pcb.port = port;
  G_pcb.remote = remote;
  G_pcb.local = local;
  G_pcb.state = LRTP_state_opening_i; // initiator
  G_pcb.start_time = lrtp_timestamp();

  // 3-way handshake with retransmissions for open_req and open_reqack
  set_recv_timeout(sd, G_pcb.rto);
  Lrtp_Packet_t pkt;
  int retries = 0;

  while (retries <= LRTP_MAX_RE_TX)
  {
    memset(&pkt, 0, sizeof(pkt));
    pkt.hdr.type = LRTP_TYPE_open_req;
    pkt.hdr.seq = G_pcb.seq_tx;
    pkt.hdr.data_size = 0;

    // Record send timestamp for RTT measurement on first transmission of open_req
    if (retries == 0)
    {
      G_pcb.tx_timestamp = lrtp_timestamp();
    }

    if (sendto(sd, (void *)&pkt, sizeof(pkt.hdr), 0,
               (struct sockaddr *)&remote, sizeof(remote)) < 0)
    {
      perror("sendto");
      close(sd);
      return LRTP_ERROR;
    }
    if (retries == 0)
      G_pcb.open_req_tx++;
    else
      G_pcb.open_req_re_tx++;

    // Try to receive open_reqack with timeout and retransmissions
    memset(&pkt, 0, sizeof(pkt));
    int n = recvfrom(sd, (void *)&pkt, sizeof(pkt), 0, NULL, NULL);

    if (n > 0)
    {
      if (pkt.hdr.type == LRTP_TYPE_open_reqack) // received expected open_reqack, proceed with handshake completion
      {
        G_pcb.open_reqack_rx++;
        G_pcb.seq_rx = pkt.hdr.seq + 1;

        // Calculate RTT and update adaptive RTO on first successful reception */
        if (retries == 0)
        {
          uint64_t now = lrtp_timestamp();
          uint32_t rtt = (uint32_t)(now - G_pcb.tx_timestamp);
          G_pcb.rtt = rtt;
          G_pcb.rto = lrtp_calculate_adaptive_rto(rtt, &G_pcb.srtt, &G_pcb.rttvar);
        }

        break;
      }
      else if (pkt.hdr.type == LRTP_TYPE_open_req) // received duplicate open_req, resend open_reqack
      {
        // Duplicate open_req received from server side: count and ignore
        G_pcb.open_req_dup_rx++;
      }
      else
      {
        // Unexpected packet type while waiting for open_reqack - ignore and continue waiting
      }
    }

    retries++;
    // Update timeout for retransmissions
    set_recv_timeout(sd, G_pcb.rto);
  }

  // If exhausted all retries without receiving open_reqack, return error
  if (retries > LRTP_MAX_RE_TX)
  {
    close(sd);
    return LRTP_ERROR;
  }

  // Send open_ack to complete 3-way handshake
  memset(&pkt, 0, sizeof(pkt));
  pkt.hdr.type = LRTP_TYPE_open_ack;
  pkt.hdr.seq = G_pcb.seq_tx;
  pkt.hdr.data_size = 0;

  if (sendto(sd, (void *)&pkt, sizeof(pkt.hdr), 0,
             (struct sockaddr *)&remote, sizeof(remote)) < 0)
  {
    perror("sendto");
    close(sd);
    return LRTP_ERROR;
  }
  G_pcb.open_ack_tx++;
  G_pcb.seq_tx++;
  G_pcb.state = LRTP_state_connected;

  return sd;
}

/*
  Close the connection associated with the given socket descriptor.
  Arguments:
    sd : socket descriptor as previously provided by lrtp_start() and lrtp_accept() or lrtp_open()

  Return:
    error - LRTP_ERROR
    success - LRTP_SUCCESS
*/
int lrtp_close(int sd)
{
  if (G_pcb.state != LRTP_state_connected) // must be in connected state to close
  {
    return LRTP_ERROR_fsm;
  }

  set_recv_timeout(sd, G_pcb.rto);

  Lrtp_Packet_t pkt;
  int retries = 0;
  int success = 0;

  while (retries <= LRTP_MAX_RE_TX)
  {
    memset(&pkt, 0, sizeof(pkt));
    pkt.hdr.type = LRTP_TYPE_close_req;
    pkt.hdr.seq = G_pcb.seq_tx++;
    pkt.hdr.data_size = 0;

    if (sendto(sd, (void *)&pkt, sizeof(pkt.hdr), 0,
               (struct sockaddr *)&G_pcb.remote, sizeof(G_pcb.remote)) < 0)
    {
      perror("sendto");
      G_pcb.finish_time = lrtp_timestamp();
      close(sd);
      return LRTP_ERROR;
    }

    if (retries == 0) // first transmission of close_req
      G_pcb.close_req_tx++;
    else // retransmissions of close_req
      G_pcb.close_req_re_tx++;

    memset(&pkt, 0, sizeof(pkt));
    int n = recvfrom(sd, (void *)&pkt, sizeof(pkt), 0, NULL, NULL);

    if (n > 0 && pkt.hdr.type == LRTP_TYPE_close_ack)
    {
      G_pcb.close_ack_rx++;
      G_pcb.state = LRTP_state_closed;
      success = 1;
      break; // exit loop cleanly
    }

    retries++;
    // Update timeout for retransmissions
    set_recv_timeout(sd, G_pcb.rto);
  }
  // If exhausted all retries without receiving close_ack, transition to closing state
  if (!success)
  {
    G_pcb.state = LRTP_state_closing_i;
  }

  // Record finish time for connection closure, whether successful or not
  G_pcb.finish_time = lrtp_timestamp();
  close(sd);

  return LRTP_SUCCESS;
}

/*
  Transmit data over the LRTP connection.

  Arguments:
    sd: socket descriptor
    data: buffer with bytestream to transmit
    data_size: number of bytes to transmit

  Return:
    error   - LRTP_ERROR
    success - number of bytes transmitted
*/
int lrtp_tx(int sd, void *data, uint16_t data_size)
{
  if (G_pcb.state != LRTP_state_connected) // must be in connected state to transmit
  {
    return LRTP_ERROR_fsm;
  }

  set_recv_timeout(sd, G_pcb.rto);

  Lrtp_Packet_t pkt;
  int retries = 0;

  while (retries <= LRTP_MAX_RE_TX)
  {
    memset(&pkt, 0, sizeof(pkt));
    pkt.hdr.type = LRTP_TYPE_data_req;
    pkt.hdr.seq = G_pcb.seq_tx++;
    pkt.hdr.data_size = data_size;
    memcpy(pkt.payload, data, data_size);

    // Record send timestamp for RTT measurement
    if (retries == 0)
    {
      G_pcb.tx_timestamp = lrtp_timestamp();
    }

    if (sendto(sd, (void *)&pkt, sizeof(pkt.hdr) + data_size, 0,
               (struct sockaddr *)&G_pcb.remote, sizeof(G_pcb.remote)) < 0)
    {
      perror("sendto");
      return LRTP_ERROR;
    }
    if (retries == 0) // first transmission of data_req
    {
      G_pcb.data_req_tx++;
      G_pcb.data_req_bytes_tx += data_size;
    }
    else // retransmissions of data_req
    {
      G_pcb.data_req_re_tx++;
      G_pcb.data_req_bytes_re_tx += data_size;
    }

    memset(&pkt, 0, sizeof(pkt));
    int n = recvfrom(sd, (void *)&pkt, sizeof(pkt), 0, NULL, NULL);

    if (n > 0 && pkt.hdr.type == LRTP_TYPE_data_ack && pkt.hdr.seq == G_pcb.seq_tx - 1)
    {
      G_pcb.data_ack_rx++;

      // Calculate RTT and update adaptive RTO
      if (retries == 0)
      {
        uint64_t now = lrtp_timestamp();
        uint32_t rtt = (uint32_t)(now - G_pcb.tx_timestamp);
        G_pcb.rtt = rtt;
        G_pcb.rto = lrtp_calculate_adaptive_rto(rtt, &G_pcb.srtt, &G_pcb.rttvar);
      }

      return data_size;
    }

    retries++;
    // Update timeout for retransmissions
    set_recv_timeout(sd, G_pcb.rto);
  }

  return LRTP_ERROR;
}

/*
  Receive data over the LRTP connection.

  Arguments:
    sd: socket descriptor
    data: buffer to store bytestream received
    data_size: size of buffer

    Return:
    error   - LRTP_ERROR
    success - number of bytes received
*/
int lrtp_rx(int sd, void *data, uint16_t data_size)
{
  if (G_pcb.state != LRTP_state_connected) // must be in connected state to receive
  {
    return LRTP_ERROR_fsm;
  }

  set_recv_timeout(sd, LRTP_RTO_FIXED * 10); // long timeout for receiving data_req, to allow for multiple retransmissions if needed

  while (1)
  {
    Lrtp_Packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    int n = recvfrom(sd, (void *)&pkt, sizeof(pkt), 0, NULL, NULL);

    if (n <= 0)
    {
      return LRTP_ERROR;
    }

    if (pkt.hdr.type == LRTP_TYPE_data_req) // received data_req, check sequence number and send appropriate ack
    {
      if (pkt.hdr.seq == G_pcb.seq_rx) // expected sequence number, accept and ack
      {
        // Calculate payload size to copy, which may be less than data_size if packet's data_size is smaller
        uint16_t payload_size = (pkt.hdr.data_size < data_size) ? pkt.hdr.data_size : data_size;
        memcpy(data, pkt.payload, payload_size);
        // Update protocol control block with received data info and advance expected sequence number
        G_pcb.seq_rx++;
        G_pcb.data_req_rx++;
        G_pcb.data_req_bytes_rx += payload_size;

        Lrtp_Packet_t ack;
        memset(&ack, 0, sizeof(ack));
        ack.hdr.type = LRTP_TYPE_data_ack;
        ack.hdr.seq = pkt.hdr.seq;

        sendto(sd, (void *)&ack, sizeof(ack.hdr), 0,
               (struct sockaddr *)&G_pcb.remote, sizeof(G_pcb.remote));
        G_pcb.data_ack_tx++;

        return payload_size;
      }
      else if (pkt.hdr.seq < G_pcb.seq_rx) // duplicate packet (already received and acked), resend ack but do not update state
      {
        G_pcb.data_req_dup_rx++;
        G_pcb.data_req_bytes_dup_rx += pkt.hdr.data_size;

        Lrtp_Packet_t ack;
        memset(&ack, 0, sizeof(ack));
        ack.hdr.type = LRTP_TYPE_data_ack;
        ack.hdr.seq = pkt.hdr.seq;

        sendto(sd, (void *)&ack, sizeof(ack.hdr), 0,
               (struct sockaddr *)&G_pcb.remote, sizeof(G_pcb.remote));
        G_pcb.data_ack_re_tx++;
      }
      continue;
    }
    else if (pkt.hdr.type == LRTP_TYPE_close_req) // received close_req, send close_ack and transition to closing state
    {
      G_pcb.close_req_rx++;
      G_pcb.state = LRTP_state_closing_i;

      Lrtp_Packet_t ack;
      memset(&ack, 0, sizeof(ack));
      ack.hdr.type = LRTP_TYPE_close_ack;
      ack.hdr.seq = G_pcb.seq_tx++;

      sendto(sd, (void *)&ack, sizeof(ack.hdr), 0,
             (struct sockaddr *)&G_pcb.remote, sizeof(G_pcb.remote));
      G_pcb.close_ack_tx++;

      return LRTP_ERROR;
    }
    else
    {
      // Received unexpected packet type while waiting for data_req - ignore and continue waiting
    }
  }
}
