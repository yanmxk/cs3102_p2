/*
  THIS FILE MUST NOT BE MODIFIED.

  CS3102 Coursework P2 : Simple, Reliable Transport Protocol (LRTP)
    saleem, Jan2024, Feb2023
    checked March 2025 (sjm55)

  Test server 3 : client-server exchange.
*/

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#include "lrtp.h"
#include "lrtp-common.h"
#include "lrtp-pcb.h"


int
main(int argc, char **argv)
{
  uid_t port = getuid(); // same as test-server-3.c
  int start_sd, sd;
  uint8_t *data;
  int c, b;
  int32_t v1, *v1_p, v2, *v2_p;
  char remote[22];
  int n = 100000; // same as test-server-3.c
  int rx_pkts = 0, tx_pkts = 0, rx_fails = 0, tx_fails = 0;

  if (argc != 1) {
    printf("usage: test-server-3\n");
    exit(0);
  }
  printf("test-server-3\n");

  if (port < 1024)
    port += 1024; // hack, avoid reserved ports, should not happen in labs

  data = malloc(LRTP_MAX_DATA_SIZE); /* LRTP_MAX_DATA_SIZE = 1280 */
  v1_p = (int32_t *) data; // first word
  v2_p = (int32_t *) &data[LRTP_MAX_DATA_SIZE - sizeof(int32_t)]; // last word

  lrtp_init();

  start_sd = lrtp_start(port);
  if (start_sd < 0) {
    printf("start() failed\n");
    exit(0);
  }
  printf("start() OK\n");

  sd = lrtp_accept(start_sd);
  if (sd < 0) {
    printf("accept() failed\n");
    exit(0);
  }
  LrtpPcb_remote(remote);
  printf("accept(): connected to %s\n", remote);

  /*
    tx and rx n packets
  */

  for (int i = 0; i < n; ++i) {

    /*
      tx : transmit a packet
    */

    memset(data, 0, LRTP_MAX_DATA_SIZE); // clear
    c = i + 1;
    *v1_p = htonl(c); // first word
    *v2_p = htonl(c); // last word
    b = lrtp_tx(sd, data, LRTP_MAX_DATA_SIZE);
    if (b < 0) {
      printf("lrtp_tx() failed (n=%d)\n", n);
      ++tx_fails;
    }

    else {  
      printf("lrtp_tx(): %d/%d pkts, %d/%d bytes.\n",
              c, n, b, LRTP_MAX_DATA_SIZE);
      ++tx_pkts;
    }


    /*
      rx : receive a packet
    */
    memset(data, 0, LRTP_MAX_DATA_SIZE); // clear
    b = lrtp_rx(sd, data, LRTP_MAX_DATA_SIZE);

    if (b < 0) {
      printf("lrtp_rx() failed (n=%d)\n", n);
      ++rx_fails;
    }

    else {
      ++rx_pkts;
  
      /* Is size as expected? */
      if (b == LRTP_MAX_DATA_SIZE)
        { v1 = ntohl(*(v1_p)); v2 = ntohl(*(v2_p)); }
      else { v1 = v2 = -1; }

      /* Is content as expected? */
#define CHECK_DATA(v_, c_) (v_ == c_ ? "(good)" : "(bad)")
      c = i + 1;
      printf("lrtp_rx(): %d/%d pkts, %d bytes %s, v1=%d %s, v2=%d %s.\n",
              c, n,
              b,  CHECK_DATA(b, LRTP_MAX_DATA_SIZE),
              v1, CHECK_DATA(v1, c),
              v2, CHECK_DATA(v2, c));
    }

  }

  /*
    finish
  */
  printf(" tx: %d/%d pkts from %s, %d fails.\n", tx_pkts, n, remote, tx_fails);
  printf(" rx: %d/%d pkts from %s, %d fails.\n", rx_pkts, n, remote, rx_fails);
  lrtp_close(sd);
  LrtpPcb_report();
  free(data);

  return 0;
}
