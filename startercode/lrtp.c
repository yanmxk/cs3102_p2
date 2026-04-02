/*
  CS3201 Coursework P2 : Lightweight Reliable Transport Protocol (LRTP).
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

/* Helper: Set socket receive timeout in microseconds */
static int set_recv_timeout(int sd, uint32_t timeout_us)
{
  struct timeval tv;
  tv.tv_sec = timeout_us / 1000000;
  tv.tv_usec = timeout_us % 1000000;
  return setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

void lrtp_init()
{
  reset_LrtpPcb();
}

/*
  port : local port number to be used for socket
  return : error - lrtp-common.h
           success - valid socket descriptor

  For use by server process.
*/
int lrtp_start(uint16_t port)
{
  int sd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sd < 0)
  {
    perror("socket");
    return LRTP_ERROR;
  }

  int reuse = 1;
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

  G_pcb.sd = sd;
  G_pcb.port = port;
  G_pcb.local = local;
  G_pcb.state = LRTP_state_listening;

  return sd;
}

/*
  sd : socket descriptor as previously provided by lrtp_start()
  return : error - lrtp-common.h
           success - sd, to indicate sd now also is "connected"

  For use by server process.
*/
int lrtp_accept(int sd)
{
  if (G_pcb.state != LRTP_state_listening)
  {
    return LRTP_ERROR_fsm;
  }

  Lrtp_Packet_t pkt;
  struct sockaddr_in remote;
  socklen_t remote_len = sizeof(remote);

  /* Set timeout for initial open_req */
  set_recv_timeout(sd, 5000000); /* 5 second timeout */

  /* Receive open_req */
  memset(&pkt, 0, sizeof(pkt));
  memset(&remote, 0, sizeof(remote));

  int n = recvfrom(sd, (void *)&pkt, sizeof(pkt), 0,
                   (struct sockaddr *)&remote, &remote_len);

  if (n < 0)
  {
    perror("recvfrom open_req");
    return LRTP_ERROR;
  }
  // 1st step in 3-ways handshake: receive open_req
  if (pkt.hdr.type != LRTP_TYPE_open_req)
  {
    return LRTP_ERROR;
  }

  G_pcb.remote = remote;
  G_pcb.seq_rx = pkt.hdr.seq + 1;
  G_pcb.start_time = lrtp_timestamp();
  G_pcb.open_req_rx++;

  /* Send open_reqack */
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
  G_pcb.open_reqack_tx++;

  /* Wait for open_ack with retransmissions */
  set_recv_timeout(sd, G_pcb.rto);
  int retries = 0;

  while (retries < LRTP_MAX_RE_TX)
  {
    memset(&pkt, 0, sizeof(pkt));
    remote_len = sizeof(G_pcb.remote);

    /* Record timestamp for first open_reqack sent (for RTT measurement) */
    if (retries == 0 && G_pcb.tx_timestamp == 0)
    {
      G_pcb.tx_timestamp = lrtp_timestamp();
    }

    n = recvfrom(sd, (void *)&pkt, sizeof(pkt), 0,
                 (struct sockaddr *)&G_pcb.remote, &remote_len);

    if (n < 0)
    {
      /* Timeout - resend open_reqack */
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

    if (pkt.hdr.type == LRTP_TYPE_open_ack)
    {
      G_pcb.open_ack_rx++;
      G_pcb.seq_tx++; /* Increment for data phase */
      G_pcb.state = LRTP_state_connected;
      
      /* Calculate RTT and update adaptive RTO on first successful reception */
      if (G_pcb.tx_timestamp > 0)
      {
        uint64_t now = lrtp_timestamp();
        uint32_t rtt = (uint32_t)(now - G_pcb.tx_timestamp);
        G_pcb.rtt = rtt;
        G_pcb.rto = lrtp_calculate_adaptive_rto(rtt, &G_pcb.srtt, &G_pcb.rttvar);
      }
      
      return sd;
    }
    else if (pkt.hdr.type == LRTP_TYPE_open_req)
    { // duplicate open_req
      G_pcb.open_req_dup_rx++;
      /* Resend open_reqack */
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
      G_pcb.open_reqack_re_tx++;
    }
  }

  return LRTP_ERROR;
}

/*
  port : local and remote port number to be used for socket
  return : error - LRTP_ERROR
           success - valid socket descriptor

  For use by client process.
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

  G_pcb.sd = sd;
  G_pcb.port = port;
  G_pcb.remote = remote;
  G_pcb.local = local;
  G_pcb.state = LRTP_state_opening_i; // initiator
  G_pcb.start_time = lrtp_timestamp();

  /* 3-way handshake */
  set_recv_timeout(sd, G_pcb.rto);
  Lrtp_Packet_t pkt;
  int retries = 0;

  while (retries <= LRTP_MAX_RE_TX)
  {
    memset(&pkt, 0, sizeof(pkt));
    pkt.hdr.type = LRTP_TYPE_open_req;
    pkt.hdr.seq = G_pcb.seq_tx;
    pkt.hdr.data_size = 0;

    /* Record send timestamp for RTT measurement */
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

    /* Try to receive open_reqack */
    memset(&pkt, 0, sizeof(pkt));
    int n = recvfrom(sd, (void *)&pkt, sizeof(pkt), 0, NULL, NULL);

    if (n > 0 && pkt.hdr.type == LRTP_TYPE_open_reqack)
    {
      G_pcb.open_reqack_rx++;
      G_pcb.seq_rx = pkt.hdr.seq + 1;
      
      /* Calculate RTT and update adaptive RTO on first successful reception */
      if (retries == 0)
      {
        uint64_t now = lrtp_timestamp();
        uint32_t rtt = (uint32_t)(now - G_pcb.tx_timestamp);
        G_pcb.rtt = rtt;
        G_pcb.rto = lrtp_calculate_adaptive_rto(rtt, &G_pcb.srtt, &G_pcb.rttvar);
      }
      
      break;
    }

    retries++;
    /* Update timeout for retransmissions */
    set_recv_timeout(sd, G_pcb.rto);
  }

  if (retries > LRTP_MAX_RE_TX)
  {
    close(sd);
    return LRTP_ERROR;
  }

  /* Send open_ack */
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
  G_pcb.seq_tx++; /* Increment for data phase */
  G_pcb.state = LRTP_state_connected;

  return sd;
}

/*
  port : local and remote port number to be used for socket
  return : error - LRTP_ERROR
           success - LRTP_SUCCESS

  For use by client process.
*/
int lrtp_close(int sd)
{
  if (G_pcb.state != LRTP_state_connected)
  {
    return LRTP_ERROR_fsm;
  }

  set_recv_timeout(sd, G_pcb.rto);

  Lrtp_Packet_t pkt;
  int retries = 0;

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
      goto close_exit;
    }
    if (retries == 0)
      G_pcb.close_req_tx++;
    else
      G_pcb.close_req_re_tx++;

    memset(&pkt, 0, sizeof(pkt));
    int n = recvfrom(sd, (void *)&pkt, sizeof(pkt), 0, NULL, NULL);

    if (n > 0 && pkt.hdr.type == LRTP_TYPE_close_ack)
    {
      G_pcb.close_ack_rx++;
      G_pcb.state = LRTP_state_closed;
      goto close_exit;
    }

    retries++;
  }

  G_pcb.state = LRTP_state_closing_i;

close_exit:
  G_pcb.finish_time = lrtp_timestamp();
  close(sd);
  return LRTP_SUCCESS;
}

/*
  sd : socket descriptor
  data : buffer with bytestream to transmit
  data_size : number of bytes to transmit
  return : error - LRTP_ERROR
           success - number of bytes transmitted
*/
int lrtp_tx(int sd, void *data, uint16_t data_size)
{
  if (G_pcb.state != LRTP_state_connected)
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

    /* Record send timestamp for RTT measurement */
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
    if (retries == 0)
    {
      G_pcb.data_req_tx++;
      G_pcb.data_req_bytes_tx += data_size;
    }
    else
    {
      G_pcb.data_req_re_tx++;
      G_pcb.data_req_bytes_re_tx += data_size;
    }

    memset(&pkt, 0, sizeof(pkt));
    int n = recvfrom(sd, (void *)&pkt, sizeof(pkt), 0, NULL, NULL);

    if (n > 0 && pkt.hdr.type == LRTP_TYPE_data_ack && pkt.hdr.seq == G_pcb.seq_tx - 1)
    {
      G_pcb.data_ack_rx++;
      
      /* Calculate RTT and update adaptive RTO */
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
    /* Update timeout for retransmissions */
    set_recv_timeout(sd, G_pcb.rto);
  }

  return LRTP_ERROR;
}

/*
  sd : socket descriptor
  data : buffer to store bytestream received
  data_size : size of buffer
  return : error - LRTP_ERROR
           success - number of bytes received
*/
int lrtp_rx(int sd, void *data, uint16_t data_size)
{
  if (G_pcb.state != LRTP_state_connected)
  {
    return LRTP_ERROR_fsm;
  }

  set_recv_timeout(sd, LRTP_RTO_FIXED * 10);

  while (1)
  {
    Lrtp_Packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    int n = recvfrom(sd, (void *)&pkt, sizeof(pkt), 0, NULL, NULL);

    if (n <= 0)
    {
      return LRTP_ERROR;
    }

    if (pkt.hdr.type == LRTP_TYPE_data_req)
    {
      if (pkt.hdr.seq == G_pcb.seq_rx)
      {
        uint16_t payload_size = (pkt.hdr.data_size < data_size) ? pkt.hdr.data_size : data_size;
        memcpy(data, pkt.payload, payload_size);
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
      else if (pkt.hdr.seq < G_pcb.seq_rx)
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
    else if (pkt.hdr.type == LRTP_TYPE_close_req)
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
  }
}
