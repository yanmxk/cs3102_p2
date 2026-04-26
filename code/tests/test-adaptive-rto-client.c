/*
  CS3102 Coursework P2 : LRTP Adaptive RTO Test Client
  
  This test program demonstrates adaptive RTO by:
  1. Measuring RTT and observing RTO adaptation
  2. Showing how adaptive RTO responds to network conditions
  3. Comparing performance metrics with fixed RTO
  
  Usage: ./test-adaptive-rto-client <server_hostname> <port> [num_packets]
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lrtp.h"
#include "lrtp-pcb.h"
#include "lrtp-common.h"

#define DEFAULT_NUM_PACKETS 10
#define PACKET_SIZE 1280

extern Lrtp_Pcb_t G_pcb;

int
main(int argc, char *argv[])
{
  if (argc < 3) {
    printf("Usage: %s <server_hostname> <port> [num_packets]\n", argv[0]);
    printf("  server_hostname : hostname or IP of LRTP server\n");
    printf("  port : port number for LRTP connection\n");
    printf("  num_packets : number of data packets to send (default: %d)\n", DEFAULT_NUM_PACKETS);
    exit(1);
  }

  const char *fqdn = argv[1];
  uint16_t port = (uint16_t)atoi(argv[2]);
  int num_packets = (argc > 3) ? atoi(argv[3]) : DEFAULT_NUM_PACKETS;

  if (num_packets <= 0) {
    num_packets = DEFAULT_NUM_PACKETS;
  }

  lrtp_init();

  printf("Adaptive RTO Test Client\n");
  printf("Connect to %s:%u\n", fqdn, port);
  printf("Sending %d packets of %d bytes each\n\n", num_packets, PACKET_SIZE);

  int sd = lrtp_open(fqdn, port);
  if (sd < 0) {
    fprintf(stderr, "lrtp_open() failed for %s:%u\n", fqdn, port);
    return 1;
  }

  printf("Connection established (adaptive RTO initialized)\n");
  printf("Initial RTO: %u us (%.3f s)\n\n", G_pcb.rto, (double)G_pcb.rto / 1000000.0);

  // Send packets and observe RTO adaptation
  uint8_t *data = (uint8_t *)malloc(PACKET_SIZE);
  if (data == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    lrtp_close(sd);
    return 1;
  }

  memset(data, 0xAA, PACKET_SIZE);

  printf("Packet |   RTT (us)   |  SRTT (us)   | RTTVAR (us)  |   RTO (us)   |  RTO (s)\n");
  printf("--------|--------------|--------------|--------------|--------------|----------\n");

  for (int i = 0; i < num_packets; i++) {
    int n = lrtp_tx(sd, data, PACKET_SIZE);
    if (n < 0) {
      fprintf(stderr, "lrtp_tx() failed at packet %d\n", i + 1);
      break;
    }

    // Display RTO metrics after each successful transmission
    printf("%7d | %12u | %12u | %12u | %12u | %.6f\n",
           i + 1,
           G_pcb.rtt,
           G_pcb.srtt,
           G_pcb.rttvar,
           G_pcb.rto,
           (double)G_pcb.rto / 1000000.0);
  }

  printf("\n");
  printf("Closing connection...\n");
  lrtp_close(sd);

  // Print final statistics
  printf("\nFinal RTO Statistics:\n");
  printf("  Smoothed RTT: %u us (%.3f ms)\n", G_pcb.srtt, (double)G_pcb.srtt / 1000.0);
  printf("  RTT Variance: %u us (%.3f ms)\n", G_pcb.rttvar, (double)G_pcb.rttvar / 1000.0);
  printf("  Final RTO: %u us (%.3f s)\n", G_pcb.rto, (double)G_pcb.rto / 1000000.0);
  printf("  Data packets TX: %llu\n", G_pcb.data_req_tx);
  printf("  Data packets RX (ACK): %llu\n", G_pcb.data_ack_rx);
  printf("  Retransmissions: %llu\n", G_pcb.data_req_re_tx);

  LrtpPcb_report();

  free(data);
  return 0;
}
