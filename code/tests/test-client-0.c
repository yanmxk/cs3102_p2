/*
  CS3102 Coursework P2 : Lightweight Reliable Transport Protocol (LRTP)
  saleem, Jan2024, Feb2023
  checked March 2025 (sjm55)
  revised March 2026 (sjm55)

  Basic test client to show use of API in lrtp.h : send then receive a single packet.
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
main(int argc, char *argv[])
{
  uid_t port = getuid(); // same as test-server-0.c
  int sd;
  uint8_t *data;
  int b;
  int32_t v1, *v1_p, v2, *v2_p;
  int c = 0x00310200; // test value, same as test-server-0.c
  char remote[22];

  if (argc < 2) {
    printf("usage: test-client-0 <fqdn> [port]\n");
    exit(0);
  }
  printf("test-client-0\n");

  if (argc > 2) {
    port = (uid_t)atoi(argv[2]);
  }
  if (port < 1024)
    port += 1024; // hack, avoid reserved ports, should not happen in labs

  lrtp_init();
  sd = lrtp_open(argv[1], port);
  if (sd < 0) {
    printf("lrtp_open() failed for %s\n", argv[1]);
    exit(0);
  }
  LrtpPcb_remote(remote);
  printf("rtp_open() : connected to %s\n", remote);

  data = malloc(LRTP_MAX_DATA_SIZE); /* LRTP_MAX_DATA_SIZE = 1280 */
  v1_p = (int32_t *) data; // first word
  v2_p = (int32_t *) &data[LRTP_MAX_DATA_SIZE - sizeof(int32_t)]; // last word

  /*
    tx : transmit a packet
  */
  memset(data, 0, LRTP_MAX_DATA_SIZE);
  *v1_p = htonl(c); // first word
  *v2_p = htonl(c); // last word
  b = lrtp_tx(sd, data, LRTP_MAX_DATA_SIZE);
  if (b != LRTP_MAX_DATA_SIZE)
    printf("lrtp_tx() failed\n");
  
  printf("lrtp_tx() : %d/%d bytes sent\n", b, LRTP_MAX_DATA_SIZE);

  /*
    rx : receive a packet
  */
  memset(data, 0, LRTP_MAX_DATA_SIZE);
  b = lrtp_rx(sd, data, LRTP_MAX_DATA_SIZE);
  if (b < 0)
    printf("lrtp_rx() failed\n");

  else {
    /* Is size as expected? */
    if (b == LRTP_MAX_DATA_SIZE)
      { v1 = ntohl(*(v1_p)); v2 = ntohl(*(v2_p)); }
    else { v1 = v2 = -1; }

    /* Is content as expected? */
#define CHECK_DATA(v_, c_) (v_ == c_ ? "(correct)" : "(incorrect)")
    printf("lrtp_rx() : %d bytes %s, v1=%d %s, v2=%d %s.\n",
           b, CHECK_DATA(b, LRTP_MAX_DATA_SIZE),
           v1, CHECK_DATA(v1, c),
           v2, CHECK_DATA(v2, c));
  }

  /*
    finish
  */  
  lrtp_close(sd);
  LrtpPcb_report();
  free(data);

  return 0;
}
