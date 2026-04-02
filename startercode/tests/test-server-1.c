/*
  THIS FILE MUST NOT BE MODIFIED.

  CS3102 Coursework P2 : Lightweight Reliable Transport Protocol (LRTP)
    saleem, Jan2024, Feb2023
    checked March 2025 (sjm55)
    revised March 2026 (sjm55)

  Test server 1 : server download. 
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
  uid_t port = getuid(); // same as test-client-1.c
  int start_sd, sd;
  uint8_t *data;
  int b;
  uint32_t *v1_p, *v2_p;
  char remote[22];
  int n = 100000; // same as test-client-1.c
  int tx_pkts = 0, tx_fails = 0;

  if (argc != 1) {
    printf("usage: test-server-1\n");
    exit(0);
  }
  printf("test-server-1\n");

  if (port < 1024)
    port += 1024; // hack, avoid reserved ports, should not happen in labs

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
  printf("accept(): %s\n", remote);

  data = calloc(1, LRTP_MAX_DATA_SIZE); /* LRTP_MAX_DATA_SIZE = 1280 */
  v1_p = (uint32_t *) data;
  v2_p = (uint32_t *) &data[LRTP_MAX_DATA_SIZE - sizeof(uint32_t)];

  /* transmit n packets */
  memset(data, 0, LRTP_MAX_DATA_SIZE); // clear
  for (int i = 0; i < n; ++i) {

    int c = i + 1;
    *v1_p = htonl(c); // first word
    *v2_p = htonl(c); // last word
    b = lrtp_tx(sd, data, LRTP_MAX_DATA_SIZE);
    if (b < 0) {
      printf("lrtp_tx() failed (n=%d)\n", n);
      ++tx_fails;
      continue;      
    }
    ++tx_pkts;

    printf("lrtp_tx(): %d/%d pkts, %d/%d bytes.\n",
           c, n, b, LRTP_MAX_DATA_SIZE);

  } // for (int i = 0; i < n; ++i)


  /*
    finish
  */
  printf(" %d/%d pkts from %s, %d fails.\n", tx_pkts, n, remote, tx_fails);
  lrtp_close(sd);
  LrtpPcb_report();
  free(data);

  return 0;
}
