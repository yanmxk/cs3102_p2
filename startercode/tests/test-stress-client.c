/*
  CS3102 Coursework P2 : LRTP Stress Test Client
  
  Stress test client that sends many packets in rapid succession.
  Tests protocol stability and RTO adaptation under load.
  
  Usage: ./test-stress-client <server_ip> [server_port] [num_packets]
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lrtp.h"
#include "lrtp-pcb.h"
#include "lrtp-common.h"

#define DEFAULT_NUM_PACKETS 50
#define PAYLOAD_SIZE 256

extern Lrtp_Pcb_t G_pcb;

int
main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <server_ip> [server_port] [num_packets]\n", argv[0]);
    return 1;
  }

  const char *server_ip = argv[1];
  uint16_t server_port = 24536;
  int num_packets = DEFAULT_NUM_PACKETS;

  if (argc > 2) {
    server_port = (uint16_t)atoi(argv[2]);
  }
  if (argc > 3) {
    num_packets = atoi(argv[3]);
    if (num_packets <= 0 || num_packets > 1000) {
      fprintf(stderr, "Number of packets must be between 1 and 1000\n");
      return 1;
    }
  }

  lrtp_init();

  printf("Stress Test Client\n");
  printf("Connecting to %s:%u\n", server_ip, server_port);
  printf("Number of packets: %d, Payload size: %d bytes\n\n", num_packets, PAYLOAD_SIZE);

  int sd = lrtp_open(server_ip, server_port);
  if (sd < 0) {
    fprintf(stderr, "lrtp_open() failed\n");
    return 1;
  }

  printf("Connection established\n\n");

  uint8_t payload[PAYLOAD_SIZE];
  for (int i = 0; i < PAYLOAD_SIZE; i++) {
    payload[i] = (uint8_t)(i % 256);
  }

  uint32_t min_rtt = UINT32_MAX, max_rtt = 0, total_rtt = 0;
  uint32_t min_rto = UINT32_MAX, max_rto = 0;
  int rto_increases = 0, rto_decreases = 0;
  uint32_t prev_rto = 0;

  printf("Packet | Previous RTO | Current RTO  | RTO Change\n");
  printf("--------|--------------|--------------|----------\n");

  for (int i = 0; i < num_packets; i++) {
    prev_rto = G_pcb.rto;

    int n = lrtp_tx(sd, payload, PAYLOAD_SIZE);
    if (n < 0) {
      fprintf(stderr, "lrtp_tx() failed at packet %d\n", i + 1);
      break;
    }

    uint32_t curr_rto = G_pcb.rto;

    /* Track RTO changes */
    if (i > 0) {
      if (curr_rto > prev_rto) {
        rto_increases++;
      } else if (curr_rto < prev_rto) {
        rto_decreases++;
      }
    }

    if (G_pcb.rtt > 0) {
      if (G_pcb.rtt < min_rtt) min_rtt = G_pcb.rtt;
      if (G_pcb.rtt > max_rtt) max_rtt = G_pcb.rtt;
      total_rtt += G_pcb.rtt;
    }

    if (curr_rto < min_rto) min_rto = curr_rto;
    if (curr_rto > max_rto) max_rto = curr_rto;

    /* Only print every 5th packet to reduce output */
    if ((i + 1) % 5 == 0) {
      int change = (int)curr_rto - (int)prev_rto;
      printf("%7d | %12u | %12u | %10d\n", i + 1, prev_rto, curr_rto, change);
    }
  }

  printf("\n");
  printf("Closing connection...\n");

  if (lrtp_close(sd) < 0) {
    fprintf(stderr, "lrtp_close() failed\n");
  }

  printf("Connection closed\n\n");

  /* Print comprehensive statistics */
  printf("Stress Test Results:\n");
  printf("  Packets sent: %d\n", num_packets);
  printf("  Packets ACK'd: %llu\n", G_pcb.data_ack_rx);
  printf("  Retransmissions: %llu\n", G_pcb.data_req_re_tx);
  printf("  Success rate: %.1f%%\n", 
         G_pcb.data_ack_rx > 0 ? (double)G_pcb.data_ack_rx / num_packets * 100 : 0.0);

  printf("\nRTT Statistics:\n");
  if (G_pcb.data_ack_rx > 0) {
    printf("  Min RTT: %u us\n", min_rtt == UINT32_MAX ? 0 : min_rtt);
    printf("  Max RTT: %u us\n", max_rtt);
    printf("  Avg RTT: %u us (%.3f ms)\n", (uint32_t)(total_rtt / G_pcb.data_ack_rx), 
           (double)(total_rtt / G_pcb.data_ack_rx) / 1000.0);
  }

  printf("\nRTO Statistics:\n");
  printf("  Min RTO: %u us (%.3f ms)\n", min_rto, (double)min_rto / 1000.0);
  printf("  Max RTO: %u us (%.3f s)\n", max_rto, (double)max_rto / 1000000.0);
  printf("  Final RTO: %u us (%.3f s)\n", G_pcb.rto, (double)G_pcb.rto / 1000000.0);
  printf("  RTO increases: %d\n", rto_increases);
  printf("  RTO decreases: %d\n", rto_decreases);

  printf("\nAdaptive RTO Performance:\n");
  printf("  Smoothed RTT: %u us (%.3f ms)\n", G_pcb.srtt, (double)G_pcb.srtt / 1000.0);
  printf("  RTT Variance: %u us (%.3f ms)\n", G_pcb.rttvar, (double)G_pcb.rttvar / 1000.0);

  LrtpPcb_report();

  return 0;
}
