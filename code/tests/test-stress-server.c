/*
  CS3102 Coursework P2 : LRTP Stress Test Server
  
  Companion to test-stress-client. Receives many packets in rapid succession.
  Monitors server-side protocol behavior under sustained data transfer.
  
  Usage: ./test-stress-server [port]
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

  printf("Stress Test Server\n");
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
  int total_bytes = 0;
  uint32_t min_rtt = UINT32_MAX, max_rtt = 0;

  printf("Receiving packets...\n");

  while (1) {
    int n = lrtp_rx(acc_sd, buffer, BUFFER_SIZE);
    if (n < 0) {
      break;
    }

    packet_count++;
    total_bytes += n;

    /* Track RTT statistics */
    if (G_pcb.rtt > 0) {
      if (G_pcb.rtt < min_rtt) min_rtt = G_pcb.rtt;
      if (G_pcb.rtt > max_rtt) max_rtt = G_pcb.rtt;
    }

    /* Print progress every 10 packets */
    if (packet_count % 10 == 0) {
      printf("  Received %d packets (%d bytes), RTO: %u us\n", 
             packet_count, total_bytes, G_pcb.rto);
    }
  }

  printf("\n");
  printf("Connection closed\n\n");

  printf("Stress Test Results (Server View):\n");
  printf("  Total packets received: %d\n", packet_count);
  printf("  Total bytes received: %d\n", total_bytes);
  printf("  Average packet size: %.1f bytes\n", 
         packet_count > 0 ? (double)total_bytes / packet_count : 0.0);

  printf("\nRTT Statistics (Server View):\n");
  if (packet_count > 0) {
    printf("  Min RTT: %u us\n", min_rtt == UINT32_MAX ? 0 : min_rtt);
    printf("  Max RTT: %u us\n", max_rtt);
  }

  printf("\nFinal RTO Statistics (Server View):\n");
  printf("  Smoothed RTT: %u us (%.3f ms)\n", G_pcb.srtt, (double)G_pcb.srtt / 1000.0);
  printf("  RTT Variance: %u us (%.3f ms)\n", G_pcb.rttvar, (double)G_pcb.rttvar / 1000.0);
  printf("  RTO: %u us (%.3f s)\n", G_pcb.rto, (double)G_pcb.rto / 1000000.0);

  LrtpPcb_report();

  free(buffer);
  close(acc_sd);
  close(sd);
  return 0;
}
