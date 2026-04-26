/*
  CS3102 Coursework P2 : LRTP Multiple Sends Test Server
  
  Companion to test-multiple-sends-client. Receives data with varying sizes.
  
  Usage: ./test-multiple-sends-server [port]
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lrtp.h"
#include "lrtp-pcb.h"
#include "lrtp-common.h"

#define BUFFER_SIZE 1280

extern Lrtp_Pcb_t G_pcb;

int
main(int argc, char *argv[])
{
  uint16_t port = 24536;

  if (argc > 1) {
    port = (uint16_t)atoi(argv[1]);
  }

  lrtp_init();

  printf("Multiple Sends Test Server\n");
  printf("Listening on port %u\n\n", port);

  int sd = lrtp_start(port);
  if (sd < 0) {
    fprintf(stderr, "lrtp_start() failed\n");
    return 1;
  }

  printf("start() OK\n");

  int acc_sd = lrtp_accept(sd);
  if (acc_sd < 0) {
    fprintf(stderr, "accept() failed\n");
    return 1;
  }

  printf("Connection accepted\n\n");

  uint8_t *buffer = (uint8_t *)malloc(BUFFER_SIZE);
  if (buffer == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return 1;
  }

  int packet_count = 0;

  printf("Payload  |   RTT (us)   |  SRTT (us)   | RTTVAR (us)  |   RTO (us)   | Bytes RX\n");
  printf("---------|--------------|--------------|--------------|--------------|----------\n");

  while (1) {
    int n = lrtp_rx(acc_sd, buffer, BUFFER_SIZE);
    if (n < 0) {
      break;
    }

    packet_count++;

    printf("%7d | %12u | %12u | %12u | %12u | %8d\n",
           packet_count,
           G_pcb.rtt,
           G_pcb.srtt,
           G_pcb.rttvar,
           G_pcb.rto,
           n);
  }

  printf("\n");
  printf("Connection closed\n");
  printf("Total payloads received: %d\n\n", packet_count);

  printf("Final RTO Statistics (Server View):\n");
  printf("  Smoothed RTT: %u us (%.3f ms)\n", G_pcb.srtt, (double)G_pcb.srtt / 1000.0);
  printf("  RTT Variance: %u us (%.3f ms)\n", G_pcb.rttvar, (double)G_pcb.rttvar / 1000.0);
  printf("  RTO: %u us (%.3f s)\n", G_pcb.rto, (double)G_pcb.rto / 1000000.0);

  LrtpPcb_report();

  free(buffer);
  close(acc_sd);
  close(sd);
  return 0;
}
